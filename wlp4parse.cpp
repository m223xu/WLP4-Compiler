// wlp4parse.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <map>
#include <utility>
#include <sstream>
#include <fstream>
using namespace std;


class WLP4Parser
{
public:
	WLP4Parser();
	~WLP4Parser();
	std::vector<std::pair<std::string, std::vector<std::string>>> derive(const std::vector<std::pair<std::string, std::string>> & sequence) const;

private:
	std::vector<std::string> terminals;
	std::vector<std::string> nonterminals;
	std::string start;
	std::vector<std::pair<std::string, std::vector<std::string>>> production_rules;
	int num_of_states;
	std::map<std::pair<int, std::string>, std::pair<std::string, int>> transducer;
};

class WLP4Exception
{
public:
	WLP4Exception(std::string info);
	~WLP4Exception();
	std::string what() const;

private:
	std::string info;
};

WLP4Exception::WLP4Exception(string info) : info(info)
{
}

WLP4Exception::~WLP4Exception()
{
}

string WLP4Exception::what() const
{
	return info;
}

WLP4Parser::WLP4Parser()
{
	fstream fin{ "wlp4.grammar" };
	size_t num_of_terminals;
	fin >> num_of_terminals;
	for (size_t i = 0; i < num_of_terminals; i++)
	{
		string s;
		fin >> s;
		terminals.emplace_back(s);
	}

	size_t num_of_nonterminals;
	fin >> num_of_nonterminals;
	for (size_t i = 0; i < num_of_nonterminals; i++)
	{
		string s;
		fin >> s;
		nonterminals.emplace_back(s);
	}

	fin >> start;

	size_t num_of_production_rules;
	fin >> num_of_production_rules;
	for (size_t i = 0; i < num_of_production_rules; i++)
	{
		string lhs;
		vector<string> rhs;
		fin >> lhs;
		string rest;
		getline(fin, rest);
		istringstream in{ rest };
		string s;
		while (in >> s)
		{
			rhs.emplace_back(s);
		}
		production_rules.emplace_back(make_pair(lhs, rhs));
	}

	fin >> num_of_states;

	size_t num_of_actions;
	fin >> num_of_actions;
	for (size_t i = 0; i < num_of_actions; i++)
	{
		int state;
		string symbol;
		string action;
		int n;
		fin >> state;
		fin >> symbol;
		fin >> action;
		fin >> n;
		transducer.emplace(make_pair(state, symbol), make_pair(action, n));
	}
}

WLP4Parser::~WLP4Parser()
{
}

vector<pair<string, vector<string>>> WLP4Parser::derive(const vector <pair<string, string>> & sequence) const
{
	int curstate = 0;
	size_t token = 0;
	stack<int> states;
	states.push(0);
	stack<string> types;
	vector<pair<string, vector<string>>> derivation;
	while (token < sequence.size())
	{
		pair<string, string> s = sequence[token];
		string type = s.first;
		string symbol = s.second;
		
		if (transducer.find(make_pair(curstate, type)) == transducer.end())
		{
			throw WLP4Exception{ "ERROR at " + to_string(token) };
		}
		else if (transducer.at(make_pair(curstate, type)).first == "reduce")
		{
			int ruleNum = transducer.at(make_pair(curstate, type)).second;
			auto production_rule = production_rules[ruleNum];
			derivation.emplace_back(production_rule);
			auto lhs = production_rule.first;
			auto rhs = production_rule.second;
			int i = rhs.size() - 1;
			while (i >= 0)
			{
				if (rhs.at(i) == types.top())
				{
					types.pop();
					states.pop();
					i--;
				}
				else
				{
					throw WLP4Exception{ "Element in stack does not match production rule" };
				}
			}
			types.push(lhs);
			curstate = transducer.at(make_pair(states.top(), lhs)).second;
			states.push(curstate);
		}
		else
		{
			derivation.emplace_back(make_pair(type, vector<string> { symbol }));
			types.push(type);
			curstate = transducer.at(make_pair(curstate, type)).second;
			states.push(curstate);
			token++;
		}
	}
	derivation.emplace_back(production_rules[0]);
	return derivation;
}

class Tree
{
	struct Node
	{
		vector<Node*> children;
		pair<string, vector<string>> info;
		void Preorder()
		{
			cout << info.first;
			for (auto s : info.second)
			{
				cout << " " << s;
			}
			cout << endl;
			for (auto child : children)
			{
				child->Preorder();
			}
		}
		~Node()
		{
			for (Node* p : children) delete p;
		}
	};
	Node* root;
public:
	Tree(vector<pair<string, vector<string>>> derivation)
	{
		stack<Node*> s;
		for (pair<string, vector<string>> r: derivation)
		{
			if (r.second.size() >= 2)
			{
				Node* newnode = new Node;
				newnode->info = r;
				int length = r.second.size();
				while (length > 0)
				{
					newnode->children.insert(newnode->children.begin(), s.top());
					s.pop();
					length--;
				}
				s.push(newnode);
			}
			else if (r.second.size() == 0)
			{
				Node* newnode = new Node;
				newnode->info = r;
				s.push(newnode);
			}
			else if (!s.empty() and r.second.at(0) == s.top()->info.first)
			{
				Node* newnode = new Node;
				newnode->info = r;
				newnode->children.insert(newnode->children.begin(), s.top());
				s.pop();
				s.push(newnode);
			}
			else
			{
				Node* newnode = new Node;
				newnode->info = r;
				s.push(newnode);
			}
		}
		root = s.top();
	}
	~Tree()
	{
		delete root;
	}
	void Preorder()
	{
		root->Preorder();
	}
};

int main()
{
	WLP4Parser wp;
	vector<pair<string, string>> sequence;
	string type = "BOF";
	string symbol = "BOF";
	sequence.emplace_back(make_pair(type, symbol));
	while (cin >> type)
	{
		cin >> symbol;
		sequence.emplace_back(make_pair(type, symbol));
	}
	type = "EOF";
	symbol = "EOF";
	sequence.emplace_back(make_pair(type, symbol));

	try
	{
		vector<pair<string, vector<string>>> derivation = wp.derive(sequence);
		Tree t(derivation);
		t.Preorder();
	}
	catch (const WLP4Exception& e)
	{
		cerr << e.what() << endl;
	}
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
