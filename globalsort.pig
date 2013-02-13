set io.sort.mb 400;
raw = LOAD 'trstrank_20120201.tsv' using PigStorage('\t') as (username, id, rank, cred);

grouped = group raw by rank;

sorted = foreach grouped {
                srt = order raw by cred;
                generate srt;
                };
flattened = foreach sorted generate flatten(srt);

store flattened into 'sorted' using PigStorage();

