// wlp4gen.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <set>
#include <map>
using namespace std;

const set<string> Terminals{ "AMP", "BECOMES", "BOF", "COMMA", "DELETE", "ELSE", "EOF", "EQ", "GE", "GT", "ID", "IF", "INT",
"LBRACE", "LBRACK", "LE", "LPAREN", "LT", "MINUS", "NE", "NEW", "NULL", "NUM", "PCT", "PLUS", "PRINTLN", "RBRACE", "RBRACK",
"RETURN", "RPAREN", "SEMI", "SLASH", "STAR", "WAIN", "WHILE" };

map<string, pair<vector<string>, map<string, pair<string, int> > > > SymbolTable;
map<string, int> fut;
int IFcounter = 0;
int WHILEcounter = 0;
int DELETEcounter = 0;

void push1(int reg)
{
	cout << "sw $" << reg << ", -4($30)" << " ;; push " << reg << endl;
	cout << "sub $30, $30, $4" << endl;
}

void pop1(int reg)
{
	cout << "add $30, $30, $4" << " ;; pop " << reg << endl;
	cout << "lw $" << reg << ", -4($30)" << endl;
}

void push2(int reg1, int reg2)
{
	cout << "sw $" << reg1 << ", -4($30)" << " ;; push " << reg1 << endl;
	cout << "sw $" << reg2 << ", -8($30)" << " ;; push " << reg2 << endl;
	cout << "sub $30, $30, $8" << endl;
}

void pop2(int reg1, int reg2)
{
	cout << "add $30, $30, $8" << endl;
	cout << "lw $" << reg1 << ", -8($30)" << " ;; pop" << reg1 << endl;
	cout << "lw $" << reg2 << ", -4($30)" << " ;; pop" << reg2 << endl;
}

class DclException
{
	string info;
public:
	DclException(string info) : info(info) {}
	~DclException() {}
	string what() const
	{
		return info;
	}
};

class TypeException
{
	string info;
public:
	TypeException(string info) : info(info) {}
	~TypeException() {}
	string what() const
	{
		return info;
	}
};

class ParseTree
{
public:
	ParseTree();
	~ParseTree();
	void buildSymbolTable();
	void typecheck();
	void code();
private:
	struct Node
	{
		vector<Node*> children;
		vector<string> tokens;
		string rule;
		Node(string rule) : rule(rule)
		{
			istringstream sin{ rule };
			string token;
			while (sin >> token)
			{
				tokens.emplace_back(token);
			}
			size_t num_of_children = tokens.size() - 1;
			if (Terminals.find(tokens[0]) != Terminals.end()) num_of_children = 0;
			for (size_t i = 0; i < num_of_children; i++)
			{
				string line;
				getline(cin, line);
				children.emplace_back(new Node{ line });
			}
		}
		~Node()
		{
			for (Node* p : children)
			{
				delete p;
			}
		}

		void collectLocalVar(map<string, pair<string, int>> & var_to_type_offset, string funcID, int & offset)
		{
			Node* type = children.at(0);
			Node* ID = children.at(1);
			string varname = ID->tokens.at(1);
			if (var_to_type_offset.find(varname) != var_to_type_offset.end())
			{
				throw DclException{ "ERROR: variable " + varname + " in " + funcID + " is declared more than once." };
			}
			if (type->rule == "type INT")
			{
				var_to_type_offset.emplace(make_pair(varname, make_pair("int", offset)));
			}
			else
			{
				var_to_type_offset.emplace(make_pair(varname, make_pair("int*", offset)));
			}
			offset -= 4;
		}

		void collectParams(map<string, pair<string, int>> & var_to_type_offset, vector<string> & paramtypes, string funcID)
		{
			if (tokens.size() != 1)
			{
				int offset = 4;
				children.at(0)->collectParamlist(var_to_type_offset, paramtypes, funcID, offset);
			}
		}

		void collectParamlist(map<string, pair<string, int>> & var_to_type_offset, vector<string> & paramtypes, string funcID, int & offset)
		{
			if (rule == "paramlist dcl COMMA paramlist")
			{
				children.at(2)->collectParamlist(var_to_type_offset, paramtypes, funcID, offset);
				children.at(0)->collectParameter(var_to_type_offset, paramtypes, funcID, offset);
			}
			else if (rule == "paramlist dcl")
			{
				children.at(0)->collectParameter(var_to_type_offset, paramtypes, funcID, offset);
			}
		}

		void collectParameter(map<string, pair<string, int>> & var_to_type_offset, vector<string> & paramtypes, string funcID, int & offset)
		{
			Node* type = children.at(0);
			Node* ID = children.at(1);
			string varname = ID->tokens.at(1);
			if (var_to_type_offset.find(varname) != var_to_type_offset.end())
			{
				throw DclException{ "ERROR: variable " + varname + " in " + funcID + " is declared more than once." };
			}
			if (type->rule == "type INT")
			{
				paramtypes.insert(paramtypes.begin(), "int");
				var_to_type_offset.emplace(make_pair(varname, make_pair("int", offset)));
			}
			else
			{
				paramtypes.insert(paramtypes.begin(), "int*");
				var_to_type_offset.emplace(make_pair(varname, make_pair("int*", offset)));
			}
			offset += 4;
		}

		void collectDcls(map<string, pair<string, int>> & var_to_type_offset, string funcID, int & offset)
		{
			if (rule != "dcls")
			{
				Node* dcl = children.at(1);
				Node* dcls = children.at(0);
				dcls->collectDcls(var_to_type_offset, funcID, offset);
				dcl->collectLocalVar(var_to_type_offset, funcID, offset);
			}
		}

		void MissingDeclaration(const map<string, pair<string, int>> & var_to_type_offset) const
		{
			if (rule == "lvalue ID" or rule == "factor ID")
			{
				Node* ID = children.at(0);
				if (var_to_type_offset.find(ID->tokens.at(1)) == var_to_type_offset.end())
				{
					throw DclException{ "ERROR: variable " + ID->tokens.at(1) + " is not declared." };
				}
				return;
			}
			for (auto child : children)
			{
				child->MissingDeclaration(var_to_type_offset);
			}
		}
		void InaccessibleFunctionCall(const map<string, pair<string, int>> & var_to_type_offset) const
		{
			if (rule == "factor ID LPAREN RPAREN" or rule == "factor ID LPAREN arglist RPAREN")
			{
				Node* ID = children.at(0);
				if (SymbolTable.find(ID->tokens.at(1)) == SymbolTable.end())
				{
					throw DclException{ "ERROR: function " + ID->tokens.at(1) + " is not declared." };
				}
				if (var_to_type_offset.find(ID->tokens.at(1)) != var_to_type_offset.end())
				{
					throw DclException{ "ERROR: " + ID->tokens.at(1) + " has already been declared as a variable." };
				}
			}
			for (auto child : children)
			{
				child->InaccessibleFunctionCall(var_to_type_offset);
			}
		}
		string TypeEval(string funcID) const
		{
			if (tokens.at(0) == "expr")
			{
				if (rule == "expr term")
				{
					return children.at(0)->TypeEval(funcID);
				}
				else if (rule == "expr expr PLUS term")
				{
					string expr = children.at(0)->TypeEval(funcID);
					string term = children.at(2)->TypeEval(funcID);
					if (expr == "int" and term == "int")
					{
						return "int";
					}
					else if (expr == "int*" and term == "int*")
					{
						throw TypeException{ "ERROR: \"int* + int*\" is not allowed." };
					}
					else
					{
						return "int*";
					}
				}
				else if (rule == "expr expr MINUS term")
				{
					string expr = children.at(0)->TypeEval(funcID);
					string term = children.at(2)->TypeEval(funcID);
					if (expr == term)
					{
						return "int";
					}
					else if (expr == "int*" and term == "int")
					{
						return "int*";
					}
					else
					{
						throw TypeException{ "ERROR: \"int - int*\" is not allowed." };
					}
				}
			}
			else if (tokens.at(0) == "lvalue")
			{
				if (rule == "lvalue ID")
				{
					return SymbolTable.at(funcID).second.at(children.at(0)->tokens.at(1)).first;
				}
				else if (rule == "lvalue STAR factor")
				{
					string factor = children.at(1)->TypeEval(funcID);
					if (factor == "int*")
					{
						return "int";
					}
					else
					{
						throw TypeException{ "ERROR: dereferencing an integer is not allowed." };
					}
				}
				else if (rule == "lvalue LPAREN lvalue RPAREN")
				{
					return children.at(1)->TypeEval(funcID);
				}
			}
			else if (tokens.at(0) == "term")
			{
				if (rule == "term factor")
				{
					return children.at(0)->TypeEval(funcID);
				}
				else if (rule == "term term STAR factor")
				{
					string term = children.at(0)->TypeEval(funcID);
					string factor = children.at(2)->TypeEval(funcID);
					if (term == "int" and factor == "int")
					{
						return "int";
					}
					else
					{
						throw TypeException{ "ERROR: \"int*\" is not allowed in multiplication." };
					}
				}
				else if (rule == "term term SLASH factor")
				{
					string term = children.at(0)->TypeEval(funcID);
					string factor = children.at(2)->TypeEval(funcID);
					if (term == "int" and factor == "int")
					{
						return "int";
					}
					else
					{
						throw TypeException{ "ERROR: \"int*\" is not allowed in division." };
					}
				}
				else if (rule == "term term PCT factor")
				{
					string term = children.at(0)->TypeEval(funcID);
					string factor = children.at(2)->TypeEval(funcID);
					if (term == "int" and factor == "int")
					{
						return "int";
					}
					else
					{
						throw TypeException{ "ERROR: \"int*\" is not allowed in modulus." };
					}
				}
			}
			else if (tokens.at(0) == "factor")
			{
				if (rule == "factor ID")
				{
					return SymbolTable.at(funcID).second.at(children.at(0)->tokens.at(1)).first;
				}
				else if (rule == "factor NUM")
				{
					return "int";
				}
				else if (rule == "factor NULL")
				{
					return "int*";
				}
				else if (rule == "factor LPAREN expr RPAREN")
				{
					return children.at(1)->TypeEval(funcID);
				}
				else if (rule == "factor AMP lvalue")
				{
					string lvalue = children.at(1)->TypeEval(funcID);
					if (lvalue == "int")
					{
						return "int*";
					}
					else
					{
						throw TypeException{ "ERROR: taking address of a pointer is not allowed" };
					}
				}
				else if (rule == "factor STAR factor")
				{
					string factor = children.at(1)->TypeEval(funcID);
					if (factor == "int*")
					{
						return "int";
					}
					else
					{
						throw TypeException{ "ERROR: dereferencing an integer is not allowed." };
					}
				}
				else if (rule == "factor NEW INT LBRACK expr RBRACK")
				{
					string expr = children.at(3)->TypeEval(funcID);
					if (expr == "int")
					{
						return "int*";
					}
					else
					{
						throw TypeException{ "ERROR: allocating memory for a pointer is not allowed" };
					}
				}
				else if (rule == "factor ID LPAREN RPAREN")
				{
					string ID = children.at(0)->tokens.at(1);

					if (fut.find(ID) == fut.end())
					{
						fut.emplace(make_pair(ID, 1));
					}
					else
					{
						fut.at(ID) = fut.at(ID) + 1;
					}

					vector<string> paramlist = SymbolTable.at(ID).first;
					if (paramlist.size() == 0)
					{
						return "int";
					}
					else
					{
						throw TypeException{ "ERROR: incorrect function call for " + ID + "." };
					}
					
				}
				else if (rule == "factor ID LPAREN arglist RPAREN")
				{
					string ID = children.at(0)->tokens.at(1);

					if (fut.find(ID) == fut.end())
					{
						fut.emplace(make_pair(ID, 1));
					}
					else
					{
						fut.at(ID) = fut.at(ID) + 1;
					}

					vector<string> paramlist = SymbolTable.at(ID).first;
					Node* arglist = children.at(2);
					size_t i = 0;
					if (paramlist.size() == 0)
					{
						throw TypeException{ "ERROR: function " + ID + " should not have arguments" };
					}
					while (arglist->rule == "arglist expr COMMA arglist")
					{
						Node* expr = arglist->children.at(0);
						arglist = arglist->children.at(2);
						if (i >= paramlist.size() - 1)
						{
							throw TypeException{ "ERROR: the number of arguments is greater than that of parameters of " + ID + "." };
						}
						string paramtype = paramlist.at(i);
						string argtype = expr->TypeEval(funcID);
						if (paramtype != argtype)
						{
							throw TypeException{ "ERROR: type does not match." };
						}
						i++;
					}
					Node* expr = arglist->children.at(0);
					if (i < paramlist.size() - 1)
					{
						throw TypeException{ "ERROR: the number of arguments is less than that of parameters of " + ID + "." };
					}
					string paramtype = paramlist.at(i);
					string argtype = expr->TypeEval(funcID);
					if (paramtype != argtype)
					{
						throw TypeException{ "ERROR: type does not match" };
					}
					return "int";
				}
			}
		}

		void stmtsTypeCheck(string funcID)
		{
			if (tokens.size() == 1)
			{
				return;
			}
			children.at(0)->stmtsTypeCheck(funcID);
			children.at(1)->stmtTypeCheck(funcID);
		}

		void stmtTypeCheck(string funcID)
		{
			if (rule == "statement lvalue BECOMES expr SEMI")
			{
				string lvalue = children.at(0)->TypeEval(funcID);
				string expr = children.at(2)->TypeEval(funcID);
				if (lvalue != expr)
				{
					throw TypeException{ "ERROR: lvalue and expr must have same type." };
				}
			}
			else if (rule == "statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE")
			{
				string expr1 = children.at(2)->children.at(0)->TypeEval(funcID);
				string expr2 = children.at(2)->children.at(2)->TypeEval(funcID);
				if (expr1 != expr2)
				{
					throw TypeException{ "ERROR: comparison requires same type." };
				}
				children.at(5)->stmtsTypeCheck(funcID);
				children.at(9)->stmtsTypeCheck(funcID);
			}
			else if (rule == "statement WHILE LPAREN test RPAREN LBRACE statements RBRACE")
			{
				string expr1 = children.at(2)->children.at(0)->TypeEval(funcID);
				string expr2 = children.at(2)->children.at(2)->TypeEval(funcID);
				if (expr1 != expr2)
				{
					throw TypeException{ "ERROR: comparison requires same type." };
				}
				children.at(5)->stmtsTypeCheck(funcID);
			}
			else if (rule == "statement PRINTLN LPAREN expr RPAREN SEMI")
			{
				string expr = children.at(2)->TypeEval(funcID);
				if (expr != "int")
				{
					throw TypeException{ "ERROR: println can only be used with an integer." };
				}
			}
			else if (rule == "statement DELETE LBRACK RBRACK expr SEMI")
			{
				string expr = children.at(3)->TypeEval(funcID);
				if (expr != "int*")
				{
					throw TypeException{ "ERROR: delete can only be used with a pointer." };
				}
			}
		}

		void dclsTypeCheck(string funcID)
		{
			if (tokens.size() == 1)
			{
				return;
			}
			if (rule == "dcls dcls dcl BECOMES NUM SEMI")
			{
				children.at(0)->dclsTypeCheck(funcID);
				if (children.at(1)->children.at(0)->tokens.size() == 3)
				{
					// INT STAR
					throw TypeException{ "ERROR: assigning a number to a pointer is not allowed." };
				}
			}
			else if (rule == "dcls dcls dcl BECOMES NULL SEMI")
			{
				children.at(0)->dclsTypeCheck(funcID);
				if (children.at(1)->children.at(0)->tokens.size() == 2)
				{
					// INT
					throw TypeException{ "ERROR: assigning a NULL to an integer typed variable is not allowed." };
				}
			}
		}


		void code(string funcID) const
		{
			if (rule == "expr term")
			{
				children.at(0)->code(funcID);
				return;
			}
			if (rule == "expr expr PLUS term")
			{
				string expr = children.at(0)->TypeEval(funcID);
				string term = children.at(2)->TypeEval(funcID);
				if (expr == "int" and term == "int")
				{
					if (children.at(0)->rule == "expr term" and children.at(0)->children.at(0)->rule == "term factor"
						and children.at(0)->children.at(0)->children.at(0)->rule == "factor NUM"
						and children.at(2)->rule == "term factor" and children.at(2)->children.at(0)->rule == "factor NUM")
					{
						string constant1 = children.at(0)->children.at(0)->children.at(0)->children.at(0)->tokens.at(1);
						string constant2 = children.at(2)->children.at(0)->children.at(0)->tokens.at(1);
						int constant = stoi(constant1) + stoi(constant2);
						cout << "lis $3" << endl;
						cout << ".word " << constant << endl;
					}
					else if (children.at(0)->rule == "expr term" and children.at(0)->children.at(0)->rule == "term factor"
						and children.at(0)->children.at(0)->children.at(0)->rule == "factor ID"
						and children.at(2)->rule == "term factor" and children.at(2)->children.at(0)->rule == "factor ID"
						and children.at(0)->children.at(0)->children.at(0)->children.at(0)->tokens.at(1) ==
						children.at(2)->children.at(0)->children.at(0)->tokens.at(1))
					{
						string varname = children.at(2)->children.at(0)->children.at(0)->tokens.at(1);
						int offset = SymbolTable.at(funcID).second.at(varname).second;
						cout << "lw $3, " + to_string(offset) + "($29)" << endl;
						cout << "add $3, $3, $3" << endl;
					}
					else
					{
						children.at(0)->code(funcID);
						push1(3);
						children.at(2)->code(funcID);
						pop1(5);
						cout << "add $3, $5, $3" << endl;
					}
				}
				else if (expr == "int*" and term == "int")
				{
					children.at(0)->code(funcID);
					push1(3);
					children.at(2)->code(funcID);
					cout << "mult $3, $4" << endl;
					cout << "mflo $3" << endl;
					pop1(5);
					cout << "add $3, $5, $3" << endl;
				}
				else if (expr == "int" and term == "int*")
				{
					children.at(0)->code(funcID);
					cout << "mult $3, $4" << endl;
					cout << "mflo $3" << endl;
					push1(3);
					children.at(2)->code(funcID);
					pop1(5);
					cout << "add $3, $5, $3" << endl;
				}
				return;
			}
			if (rule == "expr expr MINUS term")
			{

				string expr = children.at(0)->TypeEval(funcID);
				string term = children.at(2)->TypeEval(funcID);
				if (expr == "int" and term == "int")
				{
					if (children.at(0)->rule == "expr term" and children.at(0)->children.at(0)->rule == "term factor"
						and children.at(0)->children.at(0)->children.at(0)->rule == "factor NUM"
						and children.at(2)->rule == "term factor" and children.at(2)->children.at(0)->rule == "factor NUM")
					{
						string constant1 = children.at(0)->children.at(0)->children.at(0)->children.at(0)->tokens.at(1);
						string constant2 = children.at(2)->children.at(0)->children.at(0)->tokens.at(1);
						int constant = stoi(constant1) - stoi(constant2);
						cout << "lis $3" << endl;
						cout << ".word " << constant << endl;
					}
					else if (children.at(0)->rule == "expr term" and children.at(0)->children.at(0)->rule == "term factor"
						and children.at(0)->children.at(0)->children.at(0)->rule == "factor ID"
						and children.at(2)->rule == "term factor" and children.at(2)->children.at(0)->rule == "factor ID"
						and children.at(0)->children.at(0)->children.at(0)->children.at(0)->tokens.at(1) ==
						children.at(2)->children.at(0)->children.at(0)->tokens.at(1))
					{
						cout << "sub $3, $0, $0" << endl;
					}
					else
					{
						children.at(0)->code(funcID);
						push1(3);
						children.at(2)->code(funcID);
						pop1(5);
						cout << "sub $3, $5, $3" << endl;
					}
				}
				else if (expr == "int*" and term == "int")
				{
					children.at(0)->code(funcID);
					push1(3);
					children.at(2)->code(funcID);
					cout << "mult $3, $4" << endl;
					cout << "mflo $3" << endl;
					pop1(5);
					cout << "sub $3, $5, $3" << endl;
				}
				else if (expr == "int*" and term == "int*")
				{
					children.at(0)->code(funcID);
					push1(3);
					children.at(2)->code(funcID);
					pop1(5);
					cout << "sub $3, $5, $3" << endl;
					cout << "div $3, $4" << endl;
					cout << "mflo $3" << endl;
				}
				return;
			}
			if (rule == "term factor")
			{
				children.at(0)->code(funcID);
				return;
			}
			if (rule == "term term STAR factor")
			{
				if (children.at(0)->rule == "term factor" and children.at(0)->children.at(0)->rule == "factor NUM"
					and children.at(2)->rule == "factor NUM")
				{
					string constant1 = children.at(0)->children.at(0)->children.at(0)->tokens.at(1);
					string constant2 = children.at(2)->children.at(0)->tokens.at(1);
					int constant = stoi(constant1) * stoi(constant2);
					cout << "lis $3" << endl;
					cout << ".word " << constant << endl;
				}
				else if (children.at(0)->rule == "term factor" and children.at(0)->children.at(0)->rule == "factor ID"
					and children.at(2)->rule == "factor ID"
					and children.at(0)->children.at(0)->children.at(0)->tokens.at(1) ==
					children.at(2)->children.at(0)->tokens.at(1))
				{
					string varname = children.at(2)->children.at(0)->tokens.at(1);
					int offset = SymbolTable.at(funcID).second.at(varname).second;
					cout << "lw $3, " + to_string(offset) + "($29)" << endl;
					cout << "mult $3, $3" << endl;
					cout << "mflo $3" << endl;
				}
				else
				{
					children.at(0)->code(funcID);
					push1(3);
					children.at(2)->code(funcID);
					pop1(5);
					cout << "mult $5, $3" << endl;
					cout << "mflo $3" << endl;
				}
				return;
			}
			if (rule == "term term SLASH factor")
			{
				if (children.at(0)->rule == "term factor" and children.at(0)->children.at(0)->rule == "factor NUM"
					and children.at(2)->rule == "factor NUM")
				{
					string constant1 = children.at(0)->children.at(0)->children.at(0)->tokens.at(1);
					string constant2 = children.at(2)->children.at(0)->tokens.at(1);
					int constant = stoi(constant1) / stoi(constant2);
					cout << "lis $3" << endl;
					cout << ".word " << constant << endl;
				}
				else if (children.at(0)->rule == "term factor" and children.at(0)->children.at(0)->rule == "factor ID"
					and children.at(2)->rule == "factor ID"
					and children.at(0)->children.at(0)->children.at(0)->tokens.at(1) ==
					children.at(2)->children.at(0)->tokens.at(1))
				{
					cout << "add $3, $11, $0" << endl;
				}
				else
				{
					children.at(0)->code(funcID);
					push1(3);
					children.at(2)->code(funcID);
					pop1(5);
					cout << "div $5, $3" << endl;
					cout << "mflo $3" << endl;
				}
				return;
			}
			if (rule == "term term PCT factor")
			{
				if (children.at(0)->rule == "term factor" and children.at(0)->children.at(0)->rule == "factor NUM"
					and children.at(2)->rule == "factor NUM")
				{
					string constant1 = children.at(0)->children.at(0)->children.at(0)->tokens.at(1);
					string constant2 = children.at(2)->children.at(0)->tokens.at(1);
					int constant = stoi(constant1) % stoi(constant2);
					cout << "lis $3" << endl;
					cout << ".word " << constant << endl;
				}
				else if (children.at(0)->rule == "term factor" and children.at(0)->children.at(0)->rule == "factor ID"
					and children.at(2)->rule == "factor ID"
					and children.at(0)->children.at(0)->children.at(0)->tokens.at(1) ==
					children.at(2)->children.at(0)->tokens.at(1))
				{
					cout << "add $3, $0, $0" << endl;
				}
				else
				{
					children.at(0)->code(funcID);
					push1(3);
					children.at(2)->code(funcID);
					pop1(5);
					cout << "div $5, $3" << endl;
					cout << "mfhi $3" << endl;
				}
				return;
			}
			if (rule == "factor NUM")
			{
				string num = children.at(0)->tokens.at(1);
				cout << "lis $3" << endl;
				cout << ".word " << num << endl;
				return;
			}
			if (rule == "factor NULL")
			{
				cout << "add $3, $11, $0" << endl;
				return;
			}
			if (rule == "factor ID")
			{
				string varname = children.at(0)->tokens.at(1);
				int offset = SymbolTable.at(funcID).second.at(varname).second;
				cout << "lw $3, " + to_string(offset) + "($29)" << endl;
				return;
			}
			if (rule == "factor AMP lvalue")
			{
				Node* lvalue = children.at(1);
				while (lvalue->rule == "lvalue LPAREN lvalue RPAREN")
				{
					lvalue = lvalue->children.at(1);
				}
				if (lvalue->rule == "lvalue STAR factor")
				{
					lvalue->children.at(1)->code(funcID);
				}
				else if (lvalue->rule == "lvalue ID")
				{
					int offset = SymbolTable.at(funcID).second.at(lvalue->children.at(0)->tokens.at(1)).second;
					cout << "lis $3" << endl;
					cout << ".word " << offset << endl;
					cout << "add $3, $3, $29" << endl;
				}
				return;
			}
			if (rule == "factor STAR factor")
			{
				children.at(1)->code(funcID);
				cout << "lw $3, 0($3)" << endl;
				return;
			}
			if (rule == "factor LPAREN expr RPAREN")
			{
				children.at(1)->code(funcID);
				return;
			}
			if (rule == "factor NEW INT LBRACK expr RBRACK")
			{
				push2(1, 31);
				children.at(3)->code(funcID);
				cout << "add $1, $3, $0" << endl;
				cout << "jalr $13" << endl;
				pop2(31, 1);
				cout << "bne $3, $0, 1" << endl;
				cout << "add $3, $11, $0" << endl;
				return;
			}
			if (rule == "factor ID LPAREN RPAREN")
			{
				push1(31);
				cout << "lis $5" << endl;
				cout << ".word F" <<  children.at(0)->tokens.at(1) << endl;
				cout << "jalr $5" << endl;
				pop1(31);
				return;
			}
			if (rule == "factor ID LPAREN arglist RPAREN")
			{
				push1(31);
				int num_of_args = 0;
				Node* arglist = children.at(2);
				cout << ";; push args" << endl;
				while (arglist->rule == "arglist expr COMMA arglist")
				{
					num_of_args++;
					Node* expr = arglist->children.at(0);
					arglist = arglist->children.at(2);
					expr->code(funcID);
					cout << "sw $3, -" << num_of_args * 4 << "($30)" << endl;
				}
				num_of_args++;
				arglist->children.at(0)->code(funcID);
				cout << "sw $3, -" << num_of_args * 4 << "($30)" << endl;
				
				
				if (num_of_args == 1)
				{
					cout << "sub $30, $30, $4" << endl;
				}
				else if (num_of_args == 2)
				{
					cout << "sub $30, $30, $8" << endl;
				}
				else
				{
					cout << "lis $5" << endl;
					cout << ".word " << num_of_args * 4 << endl;
					cout << "sub $30, $30, $5" << endl;
				}

				cout << "lis $5" << endl;
				cout << ".word F" << children.at(0)->tokens.at(1) << endl;
				cout << "jalr $5" << endl;

				if (num_of_args == 1)
				{
					cout << "add $30, $30, $4" << endl;
				}
				else if (num_of_args == 2)
				{
					cout << "add $30, $30, $8" << endl;
				}
				else
				{
					cout << "lis $5" << endl;
					cout << ".word " << num_of_args * 4 << endl;
					cout << "add $30, $30, $5" << endl;
				}

				pop1(31);
			}
			if (rule == "statements statements statement")
			{
				children.at(0)->code(funcID);
				children.at(1)->code(funcID);
				return;
			}
			if (rule == "statement DELETE LBRACK RBRACK expr SEMI")
			{
				int localcounter = DELETEcounter++;
				push2(1, 31);
				children.at(3)->code(funcID);
				cout << "beq $3, $11, skipDelete" << localcounter << endl;
				cout << "add $1, $3, $0" << endl;
				cout << "jalr $14" << endl;
				pop2(31, 1);
				cout << "skipDelete" << localcounter << ":" << endl;
				return;
			}
			if (rule == "statement PRINTLN LPAREN expr RPAREN SEMI")
			{
				push2(1, 31);
				children.at(2)->code(funcID);
				cout << "add $1, $3, $0" << endl;
				cout << "jalr $10" << endl;
				pop2(31, 1);
				return;
			}
			if (rule == "statement lvalue BECOMES expr SEMI")
			{
				Node* lvalue = children.at(0);
				while (lvalue->rule == "lvalue LPAREN lvalue RPAREN")
				{
					lvalue = lvalue->children.at(1);
				}
				if (lvalue->rule == "lvalue ID")
				{
					int offset = SymbolTable.at(funcID).second.at(lvalue->children.at(0)->tokens.at(1)).second;
					children.at(2)->code(funcID);
					cout << "sw $3, " << offset << "($29)" << endl;
				}
				else if (lvalue->rule == "lvalue STAR factor")
				{
					children.at(2)->code(funcID);
					push1(3);
					lvalue->children.at(1)->code(funcID);
					pop1(5);
					cout << "sw $5, 0($3)" << endl;
				}
				return;
			}
			if (rule == "statement WHILE LPAREN test RPAREN LBRACE statements RBRACE")
			{
				int localcounter = WHILEcounter++;
				cout << "loop" << localcounter << ":" << endl;
				children.at(2)->code(funcID);
				cout << "beq $3, $0, endwhile" << localcounter << endl;
				children.at(5)->code(funcID);
				cout << "beq $0, $0, loop" << localcounter << endl;
				cout << "endwhile" << localcounter << ":" << endl;
				return;
			}
			if (rule == "statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE")
			{
				int localcounter = IFcounter++;
				children.at(2)->code(funcID);
				cout << "beq $3, $0, else" << localcounter << endl;
				children.at(5)->code(funcID);
				cout << "beq $0, $0, endif" << localcounter << endl;
				cout << "else" << localcounter << ":" << endl;
				children.at(9)->code(funcID);
				cout << "endif" << localcounter << ":" << endl;
				return;
			}
			if (rule == "dcls dcls dcl BECOMES NUM SEMI")
			{
				children.at(0)->code(funcID);
				string num = children.at(3)->tokens.at(1);
				cout << "lis $3" << endl;
				cout << ".word " << num << endl;
				cout << "sw $3, -4($30)" << endl;
				cout << "sub $30, $30, $4" << endl;
				return;
			}
			if (rule == "dcls dcls dcl BECOMES NULL SEMI")
			{
				children.at(0)->code(funcID);
				cout << "add $3, $11, $0" << endl;
				cout << "sw $3, -4($30)" << endl;
				cout << "sub $30, $30, $4" << endl;
				return;
			}
			if (rule == "test expr EQ expr")
			{
				children.at(0)->code(funcID);
				push1(3);
				children.at(2)->code(funcID);
				pop1(5);
				if (children.at(0)->TypeEval(funcID) == "int")
				{
					cout << "slt $6, $5, $3" << endl;
					cout << "slt $7, $3, $5" << endl;
				}
				else
				{
					cout << "sltu $6, $5, $3" << endl;
					cout << "sltu $7, $3, $5" << endl;
				}
				cout << "add $3, $6, $7" << endl;
				cout << "sub $3, $11, $3" << endl;
				return;
			}
			if (rule == "test expr NE expr")
			{
				children.at(0)->code(funcID);
				push1(3);
				children.at(2)->code(funcID);
				pop1(5);
				if (children.at(0)->TypeEval(funcID) == "int")
				{
					cout << "slt $6, $5, $3" << endl;
					cout << "slt $7, $3, $5" << endl;
				}
				else
				{
					cout << "sltu $6, $5, $3" << endl;
					cout << "sltu $7, $3, $5" << endl;
				}
				cout << "add $3, $6, $7" << endl;
				return;
			}
			if (rule == "test expr LE expr")
			{
				children.at(0)->code(funcID);
				push1(3);
				children.at(2)->code(funcID);
				pop1(5);
				if (children.at(0)->TypeEval(funcID) == "int")
				{ 
					cout << "slt $3, $3, $5" << endl;
				}
				else
				{
					cout << "sltu $3, $3, $5" << endl;
				}
				cout << "sub $3, $11, $3" << endl;
				return;
			}
			if (rule == "test expr GE expr")
			{
				children.at(0)->code(funcID);
				push1(3);
				children.at(2)->code(funcID);
				pop1(5);
				if (children.at(0)->TypeEval(funcID) == "int")
				{ 
					cout << "slt $3, $5, $3" << endl;
				}
				else
				{
					cout << "sltu $3, $5, $3" << endl;
				}
				cout << "sub $3, $11, $3" << endl;
				return;
			}
			if (rule == "test expr GT expr")
			{
				children.at(0)->code(funcID);
				push1(3);
				children.at(2)->code(funcID);
				pop1(5);
				if (children.at(0)->TypeEval(funcID) == "int")
				{
					cout << "slt $3, $3, $5" << endl;
				}
				else
				{
					cout << "sltu $3, $3, $5" << endl;
				}
				return;
			}
			if (rule == "test expr LT expr")
			{
				children.at(0)->code(funcID);
				push1(3);
				children.at(2)->code(funcID);
				pop1(5);
				if (children.at(0)->TypeEval(funcID) == "int")
				{
					cout << "slt $3, $5, $3" << endl;
				}
				else
				{
					cout << "sltu $3, $5, $3" << endl;
				}
				return;
			}
		}
	};

	Node* root;
};


ParseTree::ParseTree()
{
	string rule;
	getline(cin, rule);
	root = new Node{ rule };
}

ParseTree::~ParseTree()
{
	delete root;
}

void ParseTree::buildSymbolTable()
{
	Node* procedures = root->children.at(1);
	while (procedures->rule == "procedures procedure procedures")
	{
		Node* procedure = procedures->children.at(0);
		procedures = procedures->children.at(1);

		string funcID = procedure->children.at(1)->tokens.at(1);
		if (SymbolTable.find(funcID) != SymbolTable.end())
		{
			throw DclException{ "ERROR: function " + funcID + " is declared more than once." };
		}
		vector<string> paramtypes;
		map<string, pair<string, int>> var_to_type_offset;
		int offset = -4;

		Node* params = procedure->children.at(3);
		params->collectParams(var_to_type_offset, paramtypes, funcID);

		Node* dcls = procedure->children.at(6);
		dcls->collectDcls(var_to_type_offset, funcID, offset);

		SymbolTable.emplace(make_pair(funcID, make_pair(paramtypes, var_to_type_offset)));

		// check for use without declaration error
		Node* statements = procedure->children.at(7);
		Node* expr = procedure->children.at(9);

		statements->MissingDeclaration(var_to_type_offset);
		statements->InaccessibleFunctionCall(var_to_type_offset);
		expr->MissingDeclaration(var_to_type_offset);
		expr->InaccessibleFunctionCall(var_to_type_offset);

	}

	Node* main = procedures->children.at(0);
	string funcID = "wain";
	vector<string> paramtypes;
	map<string, pair<string, int>> var_to_type_offset;
	int offset = 4;
	
	Node* dcl2 = main->children.at(5);
	dcl2->collectParameter(var_to_type_offset, paramtypes, funcID, offset);
	
	Node* dcl1 = main->children.at(3);
	dcl1->collectParameter(var_to_type_offset, paramtypes, funcID, offset);

	offset = -4;

	Node* dcls = main->children.at(8);
	dcls->collectDcls(var_to_type_offset, funcID, offset);

	SymbolTable.emplace(make_pair(funcID, make_pair(paramtypes, var_to_type_offset)));

	// check for use without declaration error
	Node* statements = main->children.at(9);
	Node* expr = main->children.at(11);

	statements->MissingDeclaration(var_to_type_offset);
	statements->InaccessibleFunctionCall(var_to_type_offset);
	expr->MissingDeclaration(var_to_type_offset);
	expr->InaccessibleFunctionCall(var_to_type_offset);
}

void ParseTree::typecheck()
{
	Node* procedures = root->children.at(1);
	while (procedures->rule == "procedures procedure procedures")
	{
		Node* procedure = procedures->children.at(0);
		procedures = procedures->children.at(1);
		string funcID = procedure->children.at(1)->tokens.at(1);
		Node* dcls = procedure->children.at(6);
		Node* statements = procedure->children.at(7);
		Node* expr = procedure->children.at(9);
		dcls->dclsTypeCheck(funcID);
		statements->stmtsTypeCheck(funcID);
		if (expr->TypeEval(funcID) == "int*")
		{
			throw TypeException{ "ERROR: return value of " + funcID + " should be an integer." };
		}
	}
	// main function
	Node* main = procedures->children.at(0);
	string funcID = "wain";
	Node* dcls = main->children.at(8);
	Node* statements = main->children.at(9);
	Node* expr = main->children.at(11);
	dcls->dclsTypeCheck(funcID);
	statements->stmtsTypeCheck(funcID);
	if (SymbolTable.at(funcID).first.at(1) == "int*")
	{
		throw TypeException{ "ERROR: the second parameter of wain should be an integer." };
	}
	if (expr->TypeEval(funcID) == "int*")
	{
		throw TypeException{ "ERROR: return value of " + funcID + " should be an integer." };
	}
}

void ParseTree::code()
{
	cout << ".import print" << endl;
	cout << ".import init" << endl;
	cout << ".import new" << endl;
	cout << ".import delete" << endl;
	cout << "lis $4" << endl;
	cout << ".word 4" << endl;
	cout << "lis $8" << endl;
	cout << ".word 8" << endl;
	cout << "lis $11" << endl;
	cout << ".word 1" << endl;
	cout << "lis $10" << endl;
	cout << ".word print" << endl;
	cout << "lis $12" << endl;
	cout << ".word init" << endl;
	cout << "lis $13" << endl;
	cout << ".word new" << endl;
	cout << "lis $14" << endl;
	cout << ".word delete" << endl;
	Node* procedures = root->children.at(1);
	while (procedures->rule == "procedures procedure procedures")
	{
		procedures = procedures->children.at(1);
	}
	Node* main = procedures->children.at(0);
	string funcID = "wain";
	cout << "Fwain:" << endl;
	cout << "sub $30, $30, $8" << endl;
	cout << "sub $29, $30, $4" << endl;
	cout << "sw $1, 4($30)" << endl;
	Node* dcl1 = main->children.at(3);

	cout << "sw $2, 0($30)" << endl;
	Node* dcl2 = main->children.at(5);

	push1(29);

	if (dcl1->children.at(0)->tokens.size() == 2)
	{
		cout << "add $2, $0, $0" << endl;
		push1(31);
		cout << "jalr $12" << endl;
		pop1(31);
		cout << "lw $2, 0($30)" << endl;
	}
	else
	{
		push1(31);
		cout << "jalr $12" << endl;
		pop1(31);
	}

	Node* dcls = main->children.at(8);
	dcls->code(funcID);

	Node* statements = main->children.at(9);
	statements->code(funcID);

	Node* expr = main->children.at(11);
	expr->code(funcID);
	cout << "add $29, $29, $8" << endl;
	cout << "add $30, $29, $4" << endl;
	cout << "jr $31" << endl << endl;

	procedures = root->children.at(1);
	while (procedures->rule == "procedures procedure procedures")
	{
		Node* procedure = procedures->children.at(0);
		procedures = procedures->children.at(1);
		// procedure -> INT ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE
		string funcID = procedure->children.at(1)->tokens.at(1);
		if (fut.find(funcID) == fut.end())
		{
			continue;
		}
		cout << "F" << funcID << ":" << " ;;function for " << funcID << endl;
		push1(29);
		cout << "sub $29, $30, $0" << " ;; update frame pointer" << endl;

		Node* dcls = procedure->children.at(6);
		dcls->code(funcID);
		
		Node* statements = procedure->children.at(7);
		statements->code(funcID);

		Node* expr = procedure->children.at(9);
		expr->code(funcID);

		cout << "add $30, $29, $0" << endl;
		pop1(29);
		cout << "jr $31" << endl << endl;
	}
}

int main()
{
	ParseTree pt;
	try
	{
		pt.buildSymbolTable();
		for (auto func : SymbolTable)
		{
			cerr << func.first;
			for (auto paramtypes : func.second.first)
			{
				cerr << " " << paramtypes;
			}
			cerr << endl;
			for (auto var : func.second.second)
			{
				cerr << var.first << " " << var.second.first << " " << var.second.second << endl;
			}
		}
		
		pt.typecheck();
	}
	catch (const DclException & e)
	{
		cerr << e.what() << endl;
	}
	catch (const TypeException & e)
	{
		cerr << e.what() << endl;
	}
	
	pt.code();
	for (pair<string, int> p : fut)
	{
		cout << p.first << " " << p.second << endl;
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
