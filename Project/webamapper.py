results = open('weba/results_weba.txt', 'r')
mapfile = open('mapping.txt', 'r')
translatedresults = open('webaMapped', 'w')
indexmap = dict()
for line in mapfile:
	split = line.split()
	indexmap[split[0]] = split[1]
print indexmap['0']
for line in results:
	terms = line.split()
	newline = ""
	for term in terms:
		newline += " " + indexmap[term]
	translatedresults.write(newline.rstrip() + "\n")
results.close()
mapfile.close()
translatedresults.close()