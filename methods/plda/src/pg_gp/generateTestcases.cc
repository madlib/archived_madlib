#include <iostream>
#include <set>
#include <stdlib.h>

using namespace std;

int sampleWord(int topicid) {
	int rword = random() % 100;
	rword = rword + topicid * 100;
	if (rword == 0) rword = 1;
	return rword;
}

void sampleTopicDistrn(float * ret) {
	float total = 0;
	for (int i=0; i!=10; i++) {
		float nextr = random() * 1.0 / RAND_MAX;
		total += nextr;
		if (total >= 1.0) { 
			ret[i] = 1; 
			for (int j=i+1; j<10; j++) ret[j] = 1;
			break; 
		}
		if (i == 9 && total < 1.0) { ret[i] = 9; break; }
		ret[i] = total;
	}
}

void printTopicDistrn(float * ret) {
	cout << "(";
	for (int i=0; i!=9; i++)
		cout << ret[i] << ",";
	cout << ret[9] << ")";
}

int sampleTopic(float * ret) {
	float r = random() * 1.0 / RAND_MAX;
	int topic = 0;
	while (r >= ret[topic]) topic++;
	return topic;
}

set<int> alphabet;
int maxword = 0;

int main() {
	int numdocs, numwords;;
	float ret[10];

	cerr << "Please enter number of documents required.\n";
	cin >> numdocs;
	cerr << "Please enter number of words per document required.\n";
	cin >> numwords;

	cout << "DROP TABLE IF EXISTS madlib.lda_mycorpus;\n";
	cout << "CREATE TABLE madlib.lda_mycorpus ( id int4, contents int4[] ) DISTRIBUTED BY (id);\n";
	cout << "INSERT INTO madlib.lda_mycorpus VALUES \n";
	for (int i=0; i!=numdocs; i++) {
		sampleTopicDistrn(ret);
		// printTopicDistrn(ret);
		// cout << endl;
		cout << " (" << i << ", '{";
		for (int j=0; j!=numwords; j++) {
			int topic = sampleTopic(ret);
			int word = sampleWord(topic);
			alphabet.insert(word);
			if (word > maxword) maxword = word;
			//cout << "(" << topic << "," << word << ") ";
			cout << word;
			if (j == numwords-1) cout << "}')";
			else cout << ",";
		}
		if (i == numdocs-1) cout << ";\n";
		else cout << ",\n";
	}

	cout << endl << endl;
	cout << "DROP TABLE IF EXISTS madlib.lda_mydict;\n";
	cout << "CREATE TABLE madlib.lda_mydict ( dict text[] ) DISTRIBUTED RANDOMLY;\n";
	cout << "insert into madlib.lda_mydict values \n";
	cout << " ('{";
	for (int i=1; i!=maxword; i++)
		cout << i << ",";
	cout << maxword << "}');\n";
	/*
	set<int>::iterator p = alphabet.begin();
	while (p != alphabet.end()) {
		cout << *p;
		if (++p == alphabet.end()) cout << "}');\n";
		else cout << ",";
	}
	*/
}
