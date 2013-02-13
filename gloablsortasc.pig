set io.sort.mb 400;
raw = LOAD 'trstrank_20120201.tsv' using PigStorage('\t') as (username, id, rank, cred);

flattened = order raw by rank ASC, cred ASC;

store flattened into 'sortedASC' using PigStorage();
