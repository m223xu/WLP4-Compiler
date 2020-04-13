// wlp4scan.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <utility>
#include <set>
#include <array>
#include "wlp4scan.h"


const std::string MAXINT = "2147483647";

bool validInt(std::string s) {
	if (s == "0") return true;
	if (s[0] == '0') return false;
	if (s.length() > 10) return false;
	if (s.length() < 10) return true;
	return !(s > MAXINT);
}


Token::Token(Token::Kind kind, std::string lexeme) :
	kind(kind), lexeme(std::move(lexeme)) {}

Token::Kind Token::getKind() const { return kind; }
const std::string &Token::getLexeme() const { return lexeme; }

std::ostream &operator<<(std::ostream &out, const Token &tok) {
	switch (tok.getKind()) {
	case Token::KEYWORD:
		if (tok.getLexeme() == "return") out << "RETURN";
		else if (tok.getLexeme() == "if") out << "IF";
		else if (tok.getLexeme() == "else") out << "ELSE";
		else if (tok.getLexeme() == "while") out << "WHILE";
		else if (tok.getLexeme() == "println") out << "PRINTLN";
		else if (tok.getLexeme() == "wain") out << "WAIN";
		else if (tok.getLexeme() == "int") out << "INT";
		else if (tok.getLexeme() == "new") out << "NEW";
		else if (tok.getLexeme() == "delete") out << "DELETE";
		else if (tok.getLexeme() == "NULL") out << "NULL";
		else out << "ID";
		break;
	case Token::NUM:        out << "NUM";        break;
	case Token::LPAREN:     out << "LPAREN";     break;
	case Token::RPAREN:     out << "RPAREN";     break;
	case Token::LBRACE:     out << "LBRACE";     break;
	case Token::RBRACE:     out << "RBRACE";     break;
	case Token::BECOMES:    out << "BECOMES";    break;
	case Token::EQ:         out << "EQ";         break;
	case Token::NE:         out << "NE";         break;
	case Token::LT:         out << "LT";         break;
	case Token::GT:         out << "GT";         break;
	case Token::LE:         out << "LE";         break;
	case Token::GE:         out << "GE";         break;
	case Token::PLUS:       out << "PLUS";       break;
	case Token::MINUS:      out << "MINUS";      break;
	case Token::STAR:       out << "STAR";       break;
	case Token::SLASH:      out << "SLASH";      break;
	case Token::PCT:        out << "PCT";        break;
	case Token::COMMA:      out << "COMMA";      break;
	case Token::SEMI:       out << "SEMI";       break;
	case Token::LBRACK:     out << "LBRACK";     break;
	case Token::RBRACK:     out << "RBRACK";     break;
	case Token::AMP:        out << "AMP";        break;
	}
	out << " " << tok.getLexeme();

	return out;
}



ScanningFailure::ScanningFailure(std::string message) :
	message(std::move(message)) {}

const std::string &ScanningFailure::what() const { return message; }

/* Represents a DFA (which you will see formally in class later)
 * to handle the scanning
 * process. You should not need to interact with this directly:
 * it is handled through the starter code.
 */
class WLP4DFA {
public:
	enum State {
		// States that are also kinds
		NUM = 0,
		KEYWORD,
		LPAREN,
		RPAREN,
		LBRACE,
		RBRACE,
		BECOMES,
		EQ,
		NE,
		LT,
		GT,
		LE,
		GE,
		PLUS,
		MINUS,
		STAR,
		SLASH,
		PCT,
		COMMA,
		SEMI,
		LBRACK,
		RBRACK,
		AMP,
		WHITESPACE,
		COMMENT,

		// States that are not also kinds
		START,
		EXCLAMA,
		FAIL,

		// Hack to let this be used easily in arrays. This should always be the
		// final element in the enum, and should always point to the previous
		// element.
		LARGEST_STATE = FAIL
	};

private:
	/* A set of all accepting states for the DFA.
	 * Currently non-accepting states are not actually present anywhere
	 * in memory, but a list can be found in the constructor.
	 */
	std::set<State> acceptingStates;

	/*
	 * The transition function for the DFA, stored as a map.
	 */

	std::array<std::array<State, 128>, LARGEST_STATE + 1> transitionFunction;

	/*
	 * Converts a state to a kind to allow construction of Tokens from States.
	 * Throws an exception if conversion is not possible.
	 */
	Token::Kind stateToKind(State s) const {
		switch (s) {
		case KEYWORD:    return Token::KEYWORD;
		case NUM:        return Token::NUM;
		case LPAREN:     return Token::LPAREN;
		case RPAREN:     return Token::RPAREN;
		case LBRACE:     return Token::LBRACE;
		case RBRACE:     return Token::RBRACE;
		case BECOMES:    return Token::BECOMES;
		case EQ:         return Token::EQ;
		case NE:         return Token::NE;
		case LT:         return Token::LT;
		case GT:         return Token::GT;
		case LE:         return Token::LE;
		case GE:         return Token::GE;
		case PLUS:       return Token::PLUS;
		case MINUS:      return Token::MINUS;
		case STAR:       return Token::STAR;
		case SLASH:      return Token::SLASH;
		case PCT:        return Token::PCT;
		case COMMA:      return Token::COMMA;
		case SEMI:       return Token::SEMI;
		case LBRACK:     return Token::LBRACK;
		case RBRACK:     return Token::RBRACK;
		case AMP:        return Token::AMP;
		case WHITESPACE: return Token::WHITESPACE;
		case COMMENT:    return Token::COMMENT;
		default: throw ScanningFailure("ERROR: Cannot convert state to kind.");
		}
	}

	bool missingWhitespace(State first, State second) const {
		if (first == NUM and second == KEYWORD) return true;
		std::set<State> s {EQ, NE, LT, LE, GT, GE, BECOMES};
		if (s.count(first) > 0 and s.count(second) > 0) return true;
		return false;
	}

public:

	std::vector<Token> simplifiedMaximalMunch(const std::string &input) const {
		std::vector<Token> result;

		State state = start();
		State lastAcceptingState = start();
		std::string munchedInput;

		// We can't use a range-based for loop effectively here
		// since the iterator doesn't always increment.
		for (std::string::const_iterator inputPosn = input.begin();
			inputPosn != input.end();) {

			State oldState = state;
			state = transition(state, *inputPosn);

			if (!failed(state)) {
				munchedInput += *inputPosn;
				oldState = state;

				++inputPosn;
			}

			if (inputPosn == input.end() || failed(state)) {
				if (accept(oldState)) {
					if (missingWhitespace(lastAcceptingState, oldState)) {
						throw ScanningFailure{ "ERROR: Lexer error" };
					}
					result.push_back(Token(stateToKind(oldState), munchedInput));

					munchedInput = "";
					lastAcceptingState = oldState;
					state = start();
				}
				else {
					if (failed(state)) {
						munchedInput += *inputPosn;
					}
					throw ScanningFailure("ERROR: Simplified maximal munch failed on input: "
						+ munchedInput);
				}
			}
		}

		return result;
	}

	/* Initializes the accepting states for the DFA.
	 */
	WLP4DFA() {
		acceptingStates = { NUM, KEYWORD, LPAREN, RPAREN, LBRACE, RBRACE, BECOMES,
			EQ, NE, LT, GT, LE, GE, PLUS, MINUS, STAR, SLASH, PCT, COMMA, SEMI, LBRACK,
			RBRACK, AMP, WHITESPACE, COMMENT };
		// Non-accepting states are START, EXCLAMA, FAIL

		// Initialize transitions for the DFA
		for (size_t i = 0; i < transitionFunction.size(); ++i) {
			for (size_t j = 0; j < transitionFunction[0].size(); ++j) {
				transitionFunction[i][j] = FAIL;
			}
		}

		registerTransition(START, isalpha, KEYWORD);
		registerTransition(START, isdigit, NUM);
		registerTransition(NUM, isdigit, NUM);
		registerTransition(KEYWORD, isalnum, KEYWORD);
		registerTransition(START, "(", LPAREN);
		registerTransition(START, ")", RPAREN);
		registerTransition(START, "{", LBRACE);
		registerTransition(START, "}", RBRACE);
		registerTransition(START, "=", BECOMES);
		registerTransition(BECOMES, "=", EQ);
		registerTransition(START, "!", EXCLAMA);
		registerTransition(EXCLAMA, "=", NE);
		registerTransition(START, "<", LT);
		registerTransition(START, ">", GT);
		registerTransition(LT, "=", LE);
		registerTransition(GT, "=", GE);
		registerTransition(START, "+", PLUS);
		registerTransition(START, "-", MINUS);
		registerTransition(START, "*", STAR);
		registerTransition(START, "/", SLASH);
		registerTransition(START, "%", PCT);
		registerTransition(START, ",", COMMA);
		registerTransition(START, ";", SEMI);
		registerTransition(START, "[", LBRACK);
		registerTransition(START, "]", RBRACK);
		registerTransition(START, "&", AMP);
		registerTransition(START, " ", WHITESPACE);
		registerTransition(START, "\t", WHITESPACE);
		registerTransition(START, "\n", WHITESPACE);
		registerTransition(WHITESPACE, " ", WHITESPACE);
		registerTransition(WHITESPACE, "\t", WHITESPACE);
		registerTransition(WHITESPACE, "\n", WHITESPACE);
		registerTransition(SLASH, "/", COMMENT);
		registerTransition(COMMENT,
			[](int c) -> int { return c != '\n'; }, COMMENT);
	}

	// Register a transition on all chars in chars
	void registerTransition(State oldState, const std::string &chars,
		State newState) {
		for (char c : chars) {
			transitionFunction[oldState][c] = newState;
		}
	}

	// Register a transition on all chars matching test
	// For some reason the cctype functions all use ints, hence the function
	// argument type.
	void registerTransition(State oldState, int(*test)(int), State newState) {

		for (int c = 0; c < 128; ++c) {
			if (test(c)) {
				transitionFunction[oldState][c] = newState;
			}
		}
	}

	/* Returns the state corresponding to following a transition
	 * from the given starting state on the given character,
	 * or a special fail state if the transition does not exist.
	 */
	State transition(State state, char nextChar) const {
		return transitionFunction[state][nextChar];
	}

	/* Checks whether the state returned by transition
	 * corresponds to failure to transition.
	 */
	bool failed(State state) const { return state == FAIL; }

	/* Checks whether the state returned by transition
	 * is an accepting state.
	 */
	bool accept(State state) const {
		return acceptingStates.count(state) > 0;
	}

	/* Returns the starting state of the DFA
	 */
	State start() const { return START; }
};

std::vector<Token> scan(const std::string &input) {
	static WLP4DFA theDFA;

	std::vector<Token> tokens = theDFA.simplifiedMaximalMunch(input);

	std::vector<Token> newTokens;

	for (auto &token : tokens) {
		if (token.getKind() != Token::WHITESPACE
			&& token.getKind() != Token::COMMENT) {
			if (token.getKind() == Token::NUM) {
				if (validInt(token.getLexeme())) {
					newTokens.push_back(token);
				}
				else throw ScanningFailure{ "ERROR: Invalid integer" };
			}
			else newTokens.push_back(token);
		}
	}

	return newTokens;
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
