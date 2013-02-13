import json
import urllib2
import sys
import codecs

apikey = 'api_test-W1cipwpcdu9Cbd9pmm8D4Cjc469'
url = ('http://api.infochimps.com/social/network/tw/token/wordbag?apikey=api_test-W1cipwpcdu9Cbd9pmm8D4Cjc469&screen_name=')

def main():
	f= open(sys.argv[1], 'r')
	out = codecs.open('rare_words.tsv', 'w+', 'utf-8')
	for line in f:
		terms = line.split('\t')
		sn = terms[0].rstrip()
		if sn != '':
			complete = url + sn
			try:
				handle = urllib2.urlopen(complete)
			except Exception, e:
				continue
			data = handle.read()
			obj = json.loads(data)
			for x in obj['tokens']:
				try:
                                    out.write(sn + '\t' + x[0] + '\t' + str(x[1]) + '\n')
				except Exception, e:
					print x[0]
	f.close()
	out.close()
  

if __name__ == '__main__':
	main()
