

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
 * Returns standard filenames for all the data files used by GraphChi.
 * All functions expect a "basefilename".
 * You can specify environment variable "GRAPHCHI_ROOT", which is the
 * root directory for the GraphChi configuration and source directories.
 */

#ifndef GRAPHCHI_FILENAMES_DEF
#define GRAPHCHI_FILENAMES_DEF

#include <fcntl.h>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>

#include "logger/logger.hpp"

namespace graphchi {
    
#ifdef __GNUC__
#define VARIABLE_IS_NOT_USED __attribute__ ((unused))
#else
#define VARIABLE_IS_NOT_USED
#endif
    
    /**
      * Vertex data file
      */
    template <typename VertexDataType>
    static std::string filename_vertex_data(std::string basefilename) {
        std::stringstream ss;
        ss << basefilename;
        ss << "." << sizeof(VertexDataType) << "B.vout";
        return ss.str();
    }
    
    static std::string filename_degree_data(std::string basefilename)  {
        return basefilename + "_degs.bin";
    }
    
    static std::string filename_intervals(std::string basefilename, int nshards) {
        std::stringstream ss;
        ss << basefilename;
        ss << "." << nshards << ".intervals";
        return ss.str();
    }

    
    static std::string VARIABLE_IS_NOT_USED get_part_str(int p, int nshards) {
        char partstr[32];
        sprintf(partstr, ".%d_%d", p, nshards);
        return std::string(partstr);
    }
    
    template <typename EdgeDataType>
    static std::string filename_shard_edata(std::string basefilename, int p, int nshards) {
        std::stringstream ss;
        ss << basefilename;
        ss << ".edata_azv.";
        ss << "e" << sizeof(EdgeDataType) << "B.";
        ss << p << "_" << nshards;
        return ss.str();
    }
    
    static std::string filename_shard_adj(std::string basefilename, int p, int nshards) {
        std::stringstream ss;
        ss << basefilename;
        ss << ".edata_azv.";
        ss << p << "_" << nshards << ".adj";
        return ss.str();
    }
    
    /**
      * Configuration file name
      */
    static std::string filename_config();
    static std::string filename_config() {
        char * chi_root = getenv("GRAPHCHI_ROOT");
        if (chi_root != NULL) {
            return std::string(chi_root) + "/conf/graphchi.cnf";
        } else {
            return "conf/graphchi.cnf";
        }
    }
    
    /**
     * Configuration file name - local version which can
     * override the version in the version control.
     */
    static std::string filename_config_local();
    static std::string filename_config_local() {
        char * chi_root = getenv("GRAPHCHI_ROOT");
        if (chi_root != NULL) {
            return std::string(chi_root) + "/conf/graphchi.local.cnf";
        } else {
            return "conf/graphchi.local.cnf";
        }
    }
    
    
    bool shard_file_exists(std::string sname);
    bool shard_file_exists(std::string sname) {
        int tryf = open(sname.c_str(), O_RDONLY);
        if (tryf < 0) {
            return false;
        } else {
            close(tryf);
            return true;
        }
    }
    
        /**
     * Returns the number of shards if a file has been already
     * sharded or 0 if not found.
     */
    template<typename EdgeDataType>
    static int find_shards(std::string base_filename, std::string shard_string="auto") {
        int try_shard_num;
        int start_num = 0;
        int last_shard_num = 2400;
        if (shard_string == "auto") {
            start_num = 0;
        } else {
            start_num = atoi(shard_string.c_str());
        }
        
        if (start_num > 0) {
            last_shard_num = start_num;
        }
        
        for(try_shard_num=start_num; try_shard_num <= last_shard_num; try_shard_num++) {
            std::string last_shard_name = filename_shard_edata<EdgeDataType>(base_filename, try_shard_num - 1, try_shard_num);
            
            int tryf = open(last_shard_name.c_str(), O_RDONLY);
            if (tryf >= 0) {
                // Found!
                close(tryf);
                
                int nshards_candidate = try_shard_num;
                bool success = true;
                
                // Validate all relevant files exists
                for(int p=0; p < nshards_candidate; p++) {
                    std::string sname = filename_shard_edata<EdgeDataType>(base_filename, p, nshards_candidate);
                    if (!shard_file_exists(sname)) {
                        logstream(LOG_DEBUG) << "Missing shard file: " << sname << std::endl;
                        success = false;
                        break;
                    }
                    
                    sname = filename_shard_adj(base_filename, p, nshards_candidate);
                    if (!shard_file_exists(sname)) {
                        logstream(LOG_DEBUG) << "Missing shard file: " << sname << std::endl;
                        success = false;
                        break;
                    }
                }
                
                // Check degree file
                std::string degreefname = filename_degree_data(base_filename);
                if (!shard_file_exists(degreefname)) {
                    logstream(LOG_ERROR) << "Missing degree file: " << degreefname << std::endl;
                    logstream(LOG_ERROR) << "You need to preprocess (sharder) your file again!" << std::endl;
                    return 0;
                }
                
                std::string intervalfname = filename_intervals(base_filename, nshards_candidate);
                if (!shard_file_exists(intervalfname)) {
                    logstream(LOG_ERROR) << "Missing intervals file: " << intervalfname << std::endl;
                    logstream(LOG_ERROR) << "You need to preprocess (sharder) your file again!" << std::endl;
                    return 0;
                }
                
                if (!success) {
                    continue;
                }
                
                logstream(LOG_INFO) << "Detected number of shards: " << nshards_candidate << std::endl;
                logstream(LOG_INFO) << "To specify a different number of shards, use command-line parameter 'nshards'" << std::endl;
                return nshards_candidate;
            }
        }
        if (last_shard_num == start_num) {
            logstream(LOG_ERROR) << "Could not find shards with nshards = " << start_num << std::endl;
            logstream(LOG_ERROR) << "Please define 'nshards 0' or 'nshards auto' to automatically detect." << std::endl;
        }
        return 0;
    }
};
                                 
#endif

