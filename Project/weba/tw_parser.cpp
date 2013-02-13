#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace std;

#define SIZE(A) ((int)A.size())

char s[1 << 20];
map<string, int> M;

int get_id(string s)
{
	if (M.find(s) != M.end())
		return M[s];
	int size = SIZE(M);
	return M[s] = size;
}

int main()
{
	freopen("../15836067_egonet", "r", stdin);
	freopen("../twlarge.txt", "w", stdout);
	vector<int> a;
	while (scanf("%s", s) != -1)
		a.push_back(get_id(s));
	set<pair<int, int> > edges;
	for (int i = 0; i < SIZE(a); i += 2)
	{
		int u = a[i];
		int v = a[i + 1];
		if (u > v)
			swap(u, v);
		if (u == v)
			continue;
		edges.insert(make_pair(u, v));
	}
	printf("%d %d\n", SIZE(M), SIZE(edges));
	for (set<pair<int, int> >::iterator it = edges.begin(); it != edges.end(); ++it)
		printf("%d %d\n", it->first, it->second);
	ofstream myfile;
	myfile.open("../mapping.txt");
	for (map<string, int>::iterator it = M.begin(); it != M.end(); it++)
		myfile << (*it).second << " " << (*it).first << endl;
	myfile.close();
	return 0;
}