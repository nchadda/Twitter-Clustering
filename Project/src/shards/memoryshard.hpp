
/**
 * @file
 * @author  Aapo Kyrola <akyrola@cs.cmu.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * Copyright [2012] [Aapo Kyrola, Guy Blelloch, Carlos Guestrin / Carnegie Mellon University]
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 
 *
 * @section DESCRIPTION
 *
 * The memory shard. This class should only be accessed internally by the GraphChi engine.
 */

#ifndef DEF_GRAPHCHI_MEMSHARD
#define DEF_GRAPHCHI_MEMSHARD


#include <iostream>
#include <cstdio>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string>

#include "api/graph_objects.hpp"
#include "metrics/metrics.hpp"
#include "io/stripedio.hpp"
#include "graphchi_types.hpp"


namespace graphchi {


    template <typename VT, typename ET, typename svertex_t = graphchi_vertex<VT, ET>, typename ETspecial = ET>
    class memory_shard {
        
        stripedio * iomgr;
        
        std::string filename_edata;
        std::string filename_adj;

        vid_t range_st;
        vid_t range_end;
        size_t adjfilesize;
        size_t edatafilesize;
        
        size_t edgeptr;
        vid_t streaming_offset_vid;
        size_t streaming_offset; // The offset where streaming should continue
        size_t range_start_offset; // First byte for this range's vertices (used for writing only outedges)
        size_t range_start_edge_ptr;
        size_t streaming_offset_edge_ptr;
        uint8_t * adjdata;
        ET * edgedata;
        metrics &m;
        uint64_t chunkid;
        
        int edata_iosession;
        int adj_session;
        streaming_task adj_stream_session;
        
        bool is_loaded;
        
    public:
        bool only_adjacency;
        
        memory_shard(stripedio * iomgr, 
                     std::string _filename_edata,
                     std::string _filename_adj,
                     vid_t _range_start, 
                     vid_t _range_end, 
                     metrics &_m) : iomgr(iomgr), filename_edata(_filename_edata),
                    filename_adj(_filename_adj),
        range_st(_range_start), range_end(_range_end), m(_m) {
            edgedata = NULL;
            adjdata = NULL;
            only_adjacency = false;
            is_loaded = false;
            adj_session = -1;
            edata_iosession = -1;
        }
        
        ~memory_shard() {

            if (edata_iosession >= 0) {
                if (edgedata != NULL) iomgr->managed_release(edata_iosession, &edgedata);
                iomgr->close_session(edata_iosession);
            }
            if (adj_session >= 0) {
                if (adjdata != NULL) iomgr->managed_release(adj_session, &adjdata);
                iomgr->close_session(adj_session);
            }
        }
        
        void commit(bool all) {
            if (edgedata == NULL || only_adjacency) return; 
            assert(is_loaded);
            metrics_entry cm = m.start_time();
            
            /**
              * This is an optimization that is relevant only if memory shard
              * has been used in a case where only out-edges are considered.
              * Out-edges are in a continuous "window", while in-edges are 
              * scattered all over the shard
              */
            if (all) {
                iomgr->managed_pwritea_now(edata_iosession, &edgedata, edatafilesize, 0);
            } else {
                size_t last = streaming_offset_edge_ptr;
                if (last == 0){
                    // rollback
                    last = edatafilesize;
                }   
                char * bufp = ((char*)edgedata + range_start_edge_ptr);
                iomgr->managed_pwritea_now(edata_iosession, &bufp, last - range_start_edge_ptr, range_start_edge_ptr);
                
            }
            m.stop_time(cm, "memshard_commit");
            
            iomgr->managed_release(adj_session, &adjdata);
            iomgr->managed_release(edata_iosession, &edgedata);
            is_loaded = false;
        }
        
        bool loaded() {
            return is_loaded;
        }
        
        // TODO: recycle ptr!
        void load() {
            is_loaded = true;
            adjfilesize = get_filesize(filename_adj);
            edatafilesize = get_filesize(filename_edata);
            
            bool async_inedgedata_loading = !svertex_t().computational_edges();
            
#ifdef SUPPORT_DELETIONS
            async_inedgedata_loading = false;  // Currently we encode the deleted status of an edge into the edge value (should be changed!),
                                               // so we need the edge data while loading
#endif
                        
            //preada(adjf, adjdata, adjfilesize, 0);
            
            adj_session = iomgr->open_session(filename_adj, true);
            iomgr->managed_malloc(adj_session, &adjdata, adjfilesize, 0);
            adj_stream_session = streaming_task(iomgr, adj_session, adjfilesize, (char**) &adjdata);
            
            iomgr->launch_stream_reader(&adj_stream_session);            
            /* Initialize edge data asynchonous reading */
            if (!only_adjacency) {
                edata_iosession = iomgr->open_session(filename_edata, false);
                
                iomgr->managed_malloc(edata_iosession, &edgedata, edatafilesize, 0);
                if (async_inedgedata_loading) {
                    iomgr->managed_preada_async(edata_iosession, &edgedata, edatafilesize, 0);
                } else {
                    iomgr->managed_preada_now(edata_iosession, &edgedata, edatafilesize, 0);
                }
            }
        }
        
        inline void check_stream_progress(int toread, size_t pos) {
            if (adj_stream_session.curpos == adjfilesize) return;
            
            while(adj_stream_session.curpos < toread+pos) {
                usleep(20000);
                if (adj_stream_session.curpos == adjfilesize) return;
            }
        }
        
        void load_vertices(vid_t window_st, vid_t window_en, std::vector<svertex_t> & prealloc, bool inedges=true, bool outedges=true) {
            /* Find file size */
            
            m.start_time("memoryshard_create_edges");
            
            assert(adjdata != NULL);
            
            // Now start creating vertices
            uint8_t * ptr = adjdata;
            uint8_t * end = ptr + adjfilesize;
            vid_t vid = 0;
            edgeptr = 0;
            
            streaming_offset = 0;
            streaming_offset_vid = 0;
            streaming_offset_edge_ptr = 0;
            range_start_offset = adjfilesize;
            range_start_edge_ptr = edatafilesize;
            
            bool setoffset = false;
            bool setrangeoffset = false;
            while (ptr < end) {
                check_stream_progress(6, ptr-adjdata); // read at least 6 bytes
                if (!setoffset && vid > range_end) {
                    // This is where streaming should continue. Notice that because of the
                    // non-zero counters, this might be a bit off.
                    streaming_offset = ptr-adjdata;
                    streaming_offset_vid = vid;
                    streaming_offset_edge_ptr = edgeptr;
                    setoffset = true;
                }
                if (!setrangeoffset && vid>=range_st) {
                    range_start_offset = ptr-adjdata;
                    range_start_edge_ptr = edgeptr;
                    setrangeoffset = true;
                }
                
                uint8_t ns = *ptr;
                int n;
                
                ptr += sizeof(uint8_t);
                
                if (ns == 0x00) {
                    // next value tells the number of vertices with zeros
                    uint8_t nz = *ptr;
                    ptr += sizeof(uint8_t);
                    vid++;
                    vid += nz;
                    continue;
                }
                
                if (ns == 0xff) {  // If 255 is not enough, then stores a 32-bit integer after.
                    n = *((uint32_t*)ptr);
                    ptr += sizeof(uint32_t);
                } else {
                    n = ns;
                }
                svertex_t* vertex = NULL;
                
                if (vid>=window_st && vid <=window_en) { // TODO: Make more efficient
                    vertex = &prealloc[vid-window_st];
                    if (!vertex->scheduled) vertex = NULL;
                }
                check_stream_progress(n*4, ptr-adjdata);  
                while(--n>=0) {
                    bool special_edge = false;
                    vid_t target = (sizeof(ET)==sizeof(ETspecial) ? *((vid_t*) ptr) : translate_edge(*((vid_t*) ptr), special_edge));
                    ptr += sizeof(vid_t);
                    
                    
                    if (vertex != NULL && outedges) 
                    {    
                      vertex->add_outedge(target, (only_adjacency ? NULL : (ET*) &((char*)edgedata)[edgeptr]), special_edge);
                    }
                    
                    if (target >= window_st)  {
                        if (target <= window_en) {
                            /* In edge */
                            if (inedges) {
                                svertex_t & dstvertex = prealloc[target-window_st];
                                if (dstvertex.scheduled) {
                                    assert(only_adjacency ||  edgeptr < edatafilesize);
                                    dstvertex.add_inedge(vid,  (only_adjacency ? NULL : (ET*) &((char*)edgedata)[edgeptr]), special_edge);
                                    if (vertex != NULL) {
                                        dstvertex.parallel_safe = false; 
                                        vertex->parallel_safe = false;  // This edge is shared with another vertex in the same window - not safe to run in parallel.
                                    }
                                }
                            }
                        } else if (sizeof(ET) == sizeof(ETspecial)) { // Note, we cannot skip if there can be "special edges". FIXME so dirty.
                            // This vertex has no edges any more for this window, bail out
                            if (vertex == NULL) {
                                ptr += sizeof(vid_t)*n;
                                edgeptr += (n+1)*sizeof(ET);
                                break;
                            }
                        }
                    }
                    edgeptr += sizeof(ET) * !special_edge + sizeof(ETspecial) * special_edge;
                }
                vid++;
            }
            m.stop_time("memoryshard_create_edges", false);
        }
        
        size_t offset_for_stream_cont() {
            return streaming_offset;
        }
        vid_t offset_vid_for_stream_cont() {
            return streaming_offset_vid;
        }
        size_t edata_ptr_for_stream_cont() {
            return streaming_offset_edge_ptr;
        }
        
        
        
        
    };
};

#endif
    
