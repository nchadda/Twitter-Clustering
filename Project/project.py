import string 
import struct 
import networkx as nx 
import networkx.algorithms.core as core 

# Bin users by trust rank and follower credibiity. 
def bin_users():
  f=open('trstrank_0201.tsv', 'r') 
  dict={}
  i=0 
  for l in f: 
    line=l.split()
    index = len(line)==3 and 1 or 2 
    trank= int(float(line[index])/ 0.1) 
    frank= int(float(line[index+1])/5) 
    val= dict.get((trank,frank))
    dict[(trank, frank)] = val and (val+1) or 1 
    i+=1 
    if (i%2000000==0): 
      print i  
  for key in dict: 
    print key[0], key[1], dict[key] 
  
# Extract users with trust rank>1.4. 
def get_users(): 
  f=open('trstrank_0201.tsv', 'r') 
  g=open('users', 'w')
  i=0 
  for l in f: 
    line=l.split()
    index = len(line)==3 and 1 or 2 
    trank= int(float(line[index])/ 0.1)
    frank= int(float(line[index+1])/5) 
    if (trank>14): 
      print >>g, line[index-1], trank, frank
    i+=1 
    if (i%2000000==0): 
      print i 
  f.close() 
  g.close() 

# Sort users by id.  
def sort_users(): 
   g=open('users', 'r')
   users=g.readlines() 
   s_users= sorted(users, key=lambda line:int(line.split()[0])) 
   g.close() 
   h=open('users1', 'w') 
   for u in s_users: 
     print >>h, u, 

# Append user names is available. 
def add_names():
  g=open('users', 'r')
  dict={} 
  for l in g: 
    dict[l.split()[0]]="NA"
  g.close() 
  g=open('usermap/id_user.tsv', 'r') 
  i=0 
  for l in g: 
    line=l.split() 
    if (line[0] in dict): 
      dict[line[0]]= line[1] 
      i+=1 
  g.close() 
  g=open('users', 'r')
  h=open('users1', 'w') 
  for l in g:
    line=l.split() 
    print >>h, line[0], line[1], line[2], dict[l.split()[0]] 
  g.close() 
  
  

# Get edges among kernel members. 
def kernel_graph(): 
  g=open('users', 'r') 
  dict={} 
  for l in g: 
    dict[l.split()[0]] = 0 
  g.close() 
  i=0 
  f=open('twitter_rv.net', 'r') 
  h=open('kernel', 'w') 
  stats={0:0, 1:0, 2:0, 3:0} 
  for l in f: 
    i+=1 
    line=l.split() 
    b1= line[0] in dict and 1 or 0 
    b2= line[1] in dict and 1 or 0 
    b= b1*2+ b2
    stats[b] +=1
    if b==3: 
      print >>h, line[0], line[1]  
    if (i%1000000==0): 
      print i 
  f.close() 
  h.close() 

# Intersection of pagerank list and kernel. 
def intersect(top): 
  g=open('users', 'r') 
  dict={} 
  for l in g: 
    dict[l.split()[0]]=0 
  g.close() 
  cnt=0 
  g=open('pagerank.txt', 'r') 
  for l in g.readlines()[1:top]: 
    if l.split()[1] in dict: 
      cnt+=1 
  g.close() 
  print cnt 

# Convert edgelist to binary file for faster processing. 
def binary():  
  f=open('kernel', 'r') 
  g=open('kbin', 'wb') 
  ent=0 
  for l in f: 
    line=l.split() 
    s=struct.Struct('I I') 
    packed=s.pack(int(line[0]), int(line[1])) 
    g.write(packed) 
    ent+=2 
    if (ent%1000000==0): 
      print ent 
  f.close() 
  g.close() 
  print ent 

# compute degrees, build index to access binary file. 
def degrees():
  f=open('users', 'r') 
  idict={}
  odict={} 
  for l in f: 
    idict[l.split()[0]]=odict[l.split()[0]]= 0 
  f.close()   
    
  f=open('kernel', 'r') 
  i=0 
  for l in f: 
    line=l.split() 
    odict[line[0]]+=1 
    idict[line[1]]+=1 
    i+=1 
    if(i%1000000==0): 
      print i 
  f.close() 
  f=open('degs', 'w') 
  users= sorted(odict.keys(), key=lambda x: int(x))
  csum=0 
  for u in users:  
    if (u not in idict): 
      idict[u]=0 
    print >>f, u, odict[u], idict[u], csum 
    csum+= odict[u]
  f.close() 

# Returns the egonet of user and its cores. 
def get_egonet(user): 
  f=open('kbin', 'rb') 
  g=open('degs', 'r') 
  dict={} 
  for l in g:
    line=l.split() 
    dict[int(line[0])]=(int(line[1]), int(line[3])*8) 
  g.close() 

  f.seek(dict[user][1], 0)
  G=nx.DiGraph() 
  for i in range(dict[user][0]): 
    edge=struct.unpack('I I', f.read(8)) 
    G.add_edge(edge[0], edge[1]) 

  for nbr in G.neighbors(user):  
    f.seek(dict[nbr][1], 0)
    for i in range(dict[nbr][0]): 
      edge=struct.unpack('I I', f.read(8)) 
      G.add_edge(edge[0], edge[1])  
  f.close()
  print len(G.edges()), 
  for i in range(2,10): 
    G.remove_edges_from(G.selfloop_edges())
    G= core.k_core(G, i) 
    print str(nx.info(G)) 
    l= len(G.edges()) 
    print l, 
  print 
  return G 



# Returns the egonet of user and its cores. 
def count_egonet(user): 
  f=open('kbin', 'rb') 
  g=open('degs', 'r') 
  dict={} 
  for l in g:
    line=l.split() 
    dict[int(line[0])]=(int(line[1]), int(line[3])*8) 
  g.close() 

  f.seek(dict[user][1], 0)
  nlist=[]
  nodes=set() 
  for i in range(dict[user][0]): 
    edge=struct.unpack('I I', f.read(8)) 
    nlist.append(edge[1]) 
    nodes.add(edge[1]) 
    
  m=len(nlist) 
  for nbr in nlist:  
    f.seek(dict[nbr][1], 0)
    for i in range(dict[nbr][0]): 
      edge=struct.unpack('I I', f.read(8)) 
      nodes.add(edge[1])
      m+=1 
  f.close()
  print len(nodes), m

# Sample sizes of ego-nets. 
def egonet_size(): 
  f=open('samp1.txt', 'r') 
  for l in f: 
    id=int(l.split()[0]) 
    m = int(l.split()[2]) 
    if m<1000000 and m>0: 
      print id, 
      get_egonet(id)

# Filter unicode. 
def filter(str):
  anum=0 
  for c in str: 
    if c in (string.letters + string.digits): 
      anum+=1 
  if anum==len(str): 
    return 1 
  else: 
    return 0 

# Profile scrapes. 
def profiles():
  f=open('users', 'r') 
  dict={} 
  for l in f: 
    line=l.split()
    dict[line[0]]= 0 
  f.close() 
  f=open('profile_scrape', 'r') 
  wdict={}
  wcdict={} 
  ldict={} 
  i=0 
  #Strip punctuation except #tags and @mentions. 
  intab='!"$%&()*+-./:;<=>?[\\]^_`{|}~'
  outtab= ' ' *len(intab) 
  trantab = string.maketrans(intab, outtab)

  for l in f: 
    line=l.translate(trantab).lower().split(',') 
    if line[0] in dict: 
      #if len(line)>3: 
      #  ldict[line[3]]= ldict.get(line[3]) and ldict[line[3]].append(line[0]) or [line[0]] 
      if len(line)>2: 
        words=string.join(line[2:], ' ').split() 
        for w in words:
          if (filter(w)==0): 
            continue 
          wcdict[w] = wcdict.get(w) and wcdict[w]+1 or 1 
          if w in wdict: 
            wdict[w].append(line[0]) 
          else: 
            wdict[w]=[line[0]]           
    i+=1 
    if(i%10000==0): 
      print i 
  f.close() 
  g=open('wdict', 'w') 
  for w in wdict.keys(): 
    print >>g, w, wdict[w]
  g.close()
  h=open('wcdict', 'w') 
  for w in wcdict.keys(): 
    print >>h, w, wcdict[w] 
  h.close() 
  g=open('ldict', 'w')
  for w in ldict.keys(): 
    print >>g, w, ldict[w] 
  g.close() 


  
def main(): 
  #bin_users() 
  #get_users() 
  #sort_users() 
  #add_names() 
  #kernel_graph() 
  #binary()
  #degrees()
  #G=count_egonet(20632796) 
  #samp_size()
  #profiles() 
  #G = get_egonet(17427979)
  G = get_egonet(15836067)
  f = open('15836067_egonet', 'w') 
  f.write(str(len(G.nodes()))+ " " + str(len(G.edges())) + '\n')
  for a,b in G.edges():
    if a != b:
      f.write(str(a) + ' ' + str(b) + '\n')
    else:
      print 'selfloop'
  f.close()


# This is the standard boilerplate that calls the main() function.
if __name__ == '__main__':
  main()
