#ifndef CHESS_CLASSES_H
#define CHESS_CLASSES_H
#include <iostream>
#include <cstdlib>
#include <vector>
#include <map>
#include <utility>

using namespace std;

enum class Piece_type {Rook, Bishop, Knight, Queen, King, Pawn, None};

vector<Piece_type> all_piece_types {Piece_type::Pawn, Piece_type::Knight, Piece_type::Bishop, Piece_type::Rook, Piece_type::King, Piece_type::Queen};

vector<Piece_type> all_but_queen {all_piece_types.begin(),--all_piece_types.end()};

enum class Team {white, black};

Team operator!(Team t) {
	return (t == Team::white ? Team::black : Team::white);
}

struct Coordinate;

using Delta = Coordinate;

using Path_type = Piece_type;

struct Coordinate {
	int row;
	int col;
	bool operator==(Coordinate coord) {
		return (row == coord.row && col == coord.col);
	}
	void operator+=(Coordinate coord) {
		row += coord.row;
		col += coord.col;
	}
	void operator-=(Coordinate coord) {
		row -= coord.row;
		col -= coord.col;
	}
	bool on_board() {
		return (row >= 0 && row <= 7 && col >= 0 && col <= 7);
	}
	vector<Coordinate> get_adjacents() {
		vector<Coordinate> to_return;
		for (int r = -1; r <= 1; ++r) {
			for (int c = -1; c <= 1; ++c) {
				if (not (r == 0 && c == 0)) {
					to_return.push_back(Coordinate{row + r, col + c});
				}
			}
		}
		return to_return;
	}
	Coordinate(int abscissa = 0, int ordinate = 0) {
		row = abscissa;
		col = ordinate;
	}
	Delta operator+(Coordinate c) {
		return Delta{(row + c.row), (col + c.col)};
	}

	Delta operator-(Coordinate c) {
		return Delta{(row - c.row), (col - c.col)};
	}
};

const vector<Delta> pawn_barred_if_opp {Delta{1, 0}, Delta{2, 0}};

const vector<Delta> pawn_barred_if_not_opp {Delta{1, 1}, Delta{1, -1}};

const vector<Delta> others_barred_if_opp {};

const vector<Delta> others_barred_if_not_opp {};

using Path = vector<Coordinate>;

/*
| | | | | | | |
| | | |O| | | |
| | | | | | | |
| | | | | | | |
| | | | |X| | |
| | | | | | | |
| | | | | | | |
| | | | | | | |
*/

template <typename element>
vector<element> combine(const vector<element>& v1, const vector<element>& v2) {
	vector<element> vr = v2;
	vr.insert(vr.begin(), v1.begin(), v1.end());
	return vr;
}

struct Piece {
	Coordinate position;
	Team team;
	const vector<Delta>* barred_if_opp;
	const vector<Delta>* barred_if_not_opp;
	bool has_moved = false;
	Piece_type type;
	Delta distance_to(Coordinate destination) {
		Delta delta{destination.row - position.row, destination.col - position.col};
		if (team == Team::black) {           // black should start at row 0
			delta.col *= -1;
		} else {
			delta.row *= -1;
		}
		return delta;
	}
	Delta abs_distance_to(Coordinate destination) {
		return Delta{abs(destination.row - position.row), abs(destination.col - position.col)};
	}
	Path path_to(Coordinate destination) {
		// BPS: sgn() function (not in C++ std library) would simplify this.
		// BPS: Question: Is there an advantage to instantiating paths in this way?
		// BPS: Question: Would it be simpler to have each piece type override this? I notice that each call
		//      site has validation before the call to path_to.
		int row_inc = ((destination.row - position.row) == 0 ? 0 : ((destination.row - position.row) / abs(destination.row - position.row)));
		int col_inc = ((destination.col - position.col) == 0 ? 0 : ((destination.col - position.col) / abs(destination.col - position.col)));
		Path path;
		for (int prow = position.row + row_inc, pcol = position.col + col_inc, cnt = 1; 
			cnt <= abs(destination.row - position.row); 
			prow += row_inc, pcol += col_inc, cnt++) 
		{
			path.push_back(Coordinate{prow, pcol});
		}
		return path;
	}
	bool path_valid(Path);
	bool result_valid(bool);
	virtual Path delta_valid(Delta move) = 0;
	Piece(Coordinate pos, Team tm, Piece_type t, const vector<Delta>* b_i_o = &others_barred_if_opp, const vector<Delta>* b_i_n_o = &others_barred_if_not_opp) 
		:position{pos}, team{tm}, type{t}, barred_if_opp{b_i_o}, barred_if_not_opp{b_i_n_o} {}
};

struct Pawn : public Piece {
		Pawn(Coordinate pos, Team tm):Piece{pos, tm, Piece_type::Pawn, &pawn_barred_if_opp, &pawn_barred_if_not_opp} {}
		Path delta_valid(Delta destination) {
			Delta d_t = distance_to(destination);
			//cout << "Distance to = (" << d_t.row << "," << d_t.col << ")\n";
			if (destination.on_board() && ((d_t.row == 1 && d_t.col == 0)
						        || (d_t.row == 1 && abs(d_t.col) == 1) 
						        || (d_t.row == 2 && d_t.col == 0 && (not has_moved)))) {
				// BPS: In the d_t.row == 2 case, path will not include the position directly in front
				// of the pawn. Won't this pose a problem in path_valid, which checks only positions in
				// path for occupancy?
				return Path{destination};
			} else {
				return Path{};
			}
		}
};

struct Knight : public Piece {
		Knight(Coordinate pos, Team tm):Piece{pos, tm, Piece_type::Knight} {}
		Path delta_valid(Delta destination) {
			Delta abs_d_t = abs_distance_to(destination);
			if (destination.on_board() && abs_d_t.row > 0 && abs_d_t.col > 0 && abs_d_t.row < 3 && abs_d_t.col < 3 
			    && (abs_d_t.row / abs_d_t.col == 2 || abs_d_t.col / abs_d_t.row == 2)) {
				return Path{destination};
			} else {
				return Path{};
			}
		}
};

struct Bishop : public Piece {
		Bishop(Coordinate pos, Team tm):Piece{pos, tm, Piece_type::Bishop} {}
		Path delta_valid(Delta destination) {
			Delta abs_d_t = abs_distance_to(destination);
			if (destination.on_board() && abs_d_t.row != 0 && abs_d_t.row == abs_d_t.col) {
				return path_to(destination);
			} else {
				return Path{};
			}
		}
};

struct Rook : public Piece {
		Rook(Coordinate pos, Team tm):Piece{pos, tm, Piece_type::Rook} {}
		Path delta_valid(Delta destination) {
			Delta abs_d_t = abs_distance_to(destination);
			if (destination.on_board() && ((abs_d_t.row == 0 && abs_d_t.col != 0) || (abs_d_t.row != 0 && abs_d_t.col == 0))) {
				return path_to(destination);
			} else {
				return Path{};
			}
		}
};

struct Queen: public Piece {
		Queen(Coordinate pos, Team tm):Piece{pos, tm, Piece_type::Queen} {}
		Path delta_valid(Delta destination) {
			Delta abs_d_t = abs_distance_to(destination);
			if (destination.on_board() && ((abs_d_t.row == 0 && abs_d_t.col != 0) || (abs_d_t.row != 0 && abs_d_t.col == 0) || (abs_d_t.row != 0 && abs_d_t.row == abs_d_t.col))) {
				return path_to(destination);
			} else {
				return Path{};
			}
		}
};

struct King: public Piece {
		King(Coordinate pos, Team tm):Piece{pos, tm, Piece_type::King} {}
		Path delta_valid(Delta destination) {
			Delta abs_d_t = abs_distance_to(destination);
			if (destination.on_board() && abs_d_t.row < 2 && abs_d_t.col < 2 && (abs_d_t.row > 0 || abs_d_t.col > 0)) {
				return Path{destination};
			} else {
				return Path{};
			}
		}
};

Piece* create_piece(Piece_type pt, Coordinate pos, Team tm) {
	switch (pt) {
		case Piece_type::Rook:
			return new Rook(pos, tm);
		case Piece_type::Bishop:
			return new Bishop(pos, tm);
		case Piece_type::Knight:
			return new Knight(pos, tm);
		case Piece_type::Queen:
			return new Queen(pos, tm);
		case Piece_type::King:
			return new King(pos, tm);
		case Piece_type::Pawn:
			return new Pawn(pos, tm);
	}
}

struct Board {
	vector<vector<Piece*>> board;
	Path check_lane;
	Coordinate black_king = {0, 4};
	Coordinate white_king = {7, 4};
	/*void swap(Coordinate c1, Coordinate c2) {
		Piece* saved_val = board[c1.row][c1.col];
		board[c1.row][c1.col] = board[c2.row][c2.col];
		board[c2.row][c2.col] = saved_val;
	}*/
	void display() {
		map<Team, map<Piece_type, string>> piece_strings {  { Team::black, {{Piece_type::Pawn, " \u2659 "},
			                                                            {Piece_type::Knight, " \u2658 "},
								                    {Piece_type::Bishop, " \u2657 "},
								                    {Piece_type::Rook, " \u2656 "},
								                    {Piece_type::Queen, " \u2655 "},
								                    {Piece_type::King, " \u2654 "}} },
			                                            { Team::white, {{Piece_type::Pawn, " \u265F "},
			                                                            {Piece_type::Knight, " \u265E "},
								                    {Piece_type::Bishop, " \u265D "},
								                    {Piece_type::Rook, " \u265C "},
								                    {Piece_type::Queen, " \u265B "},
								                    {Piece_type::King, " \u265A "}} }  };
		string spacer = "\n   -----------------------------------------\n";
		cout << "     0    1    2    3    4    5    6    7";
		cout << spacer;
		int row_cnt = 0;
		for (auto row : board) {
			cout << row_cnt << "  |";
			++row_cnt;
			for (auto piece : row) {
				cout << (piece ? piece_strings[piece->team][piece->type] : "   ") << " |";
			}
			cout << spacer;
		}

	}
	Coordinate get_king(Team tm) {
		return (tm == Team::black ? black_king : white_king);
	}
	Path search_path(Path_type path_type, Coordinate search_from, Team mover_team, bool finding_threat, bool need_path);
	Path is_threatened(Team threatening, Coordinate threatened_coord, bool need_path) {
		Path p;
		// BPS: Look for paths between threatening's pieces and the threatened_coord. Search is for one piece
		//      type at a time.
		// BPS: All but queen reflects fact that combination of rook and bishop are used to obviate need for
		//      queen tests.
		for (Piece_type pt : all_but_queen) {
			p = search_path(pt, threatened_coord, threatening, true, need_path);
			if (!p.empty()) {
				return p;
			}
		}
		return p;
	}
	bool checkmate_check(Team threatened) {
		Coordinate king = get_king(threatened);
		if (none_of(check_lane.begin(), check_lane.end(), [&](Coordinate c) { return !is_threatened(threatened, c, false).empty(); })) {
			Piece* save = board[king.row][king.col];
			board[king.row][king.col] = nullptr;
			vector<Coordinate> adjacents = king.get_adjacents();
			if (all_of(adjacents.begin(), adjacents.end(), [&](Coordinate c) { return !is_threatened(!threatened, c, false).empty(); })) {
				board[king.row][king.col] = save;
				return true;
			}
			board[king.row][king.col] = save;
		}
		return false;
	}
	bool stalemate_check(Team threatened) {
		for (int row = 0; row <= 7; ++row) {
			for (int col = 0; col <= 7; ++col) {
				Piece* piece = board[row][col];
				if (piece && piece->team == threatened) {
					for (Piece_type pt : all_piece_types) {
						if (!search_path(pt, Coordinate(row,col), threatened, false, false).empty()) {
							return false;
						}
					}
				}
			}
		}
		return true;
	}
	pair<Piece*, bool> move(Coordinate c1, Coordinate c2) {
		Piece* to_move = board[c1.row][c1.col];
		bool old_has_moved = to_move->has_moved;
		if (to_move->type == Piece_type::King) {
			// BPS: Parens around the assignments are not necessary in C++, but this construction might
			//      cause even some experienced programmers to resort to the operator precedence page. Don't
			//      know if that's a good or back thing. Another possibility:
			//      *(Coordinate *)(to_move->team == Team::black ? &black_king : &white_king) = c2;
			(to_move->team == Team::black) ? black_king = c2 : white_king = c2;
		}
		// BPS: Question: What is the rationale for the denormalization
		//      inherent in strategy that has both the board and the
		//      individual pieces maintain piece position? Not saying
		//      there's not a good rationale, and perhaps it will become
		//      clearer as I review further...
		Piece* to_return = board[c2.row][c2.col];
		board[c2.row][c2.col] = to_move;
		to_move->position = c2;
		to_move->has_moved = true;
		board[c1.row][c1.col] = nullptr;
		return make_pair(to_return, old_has_moved);
	}
	void undo_move(Coordinate c1, Coordinate c2, Piece* replace_piece, bool replace_has_moved) {
		Piece* moved_piece = board[c2.row][c2.col];
		if (moved_piece && moved_piece->type == Piece_type::King) {
			(moved_piece->team == Team::black) ? black_king = c1 : white_king = c1;
		}
		//change has_moved here! MAYBE!
		board[c1.row][c1.col] = moved_piece;
		board[c1.row][c1.col]->position = c1;
		board[c1.row][c1.col]->has_moved = replace_has_moved;
		board[c2.row][c2.col] = replace_piece;
	}
	vector<Piece*>& operator[](int row) {
		return board[row];
	}
	Piece*& get_at(Coordinate c) {
		return board[c.row][c.col];
	}
	Board() {
		vector<Piece_type> edge_row { Piece_type::Rook, Piece_type::Bishop, Piece_type::Knight, Piece_type::Queen, 
			                      Piece_type::King, Piece_type::Knight, Piece_type::Bishop, Piece_type::Rook };
		vector<vector<Piece_type>> order { edge_row,
						   vector<Piece_type>(8, Piece_type::Pawn),
					           vector<Piece_type>(8, Piece_type::None),
					           vector<Piece_type>(8, Piece_type::None),
					           vector<Piece_type>(8, Piece_type::None),
					           vector<Piece_type>(8, Piece_type::None),
					           vector<Piece_type>(8, Piece_type::Pawn),
						   edge_row };
		Team tm = Team::black;
		for (int row = 0; row <= 7; ++row) {
			board.push_back(vector<Piece*>());
			if (row == 2) { tm = Team::white; }
			for (int col = 0; col <= 7; ++col) {
				Piece_type el = order[row][col];
				if (el == Piece_type::None) {
					board[row].push_back(nullptr);
				} else {
					board[row].push_back(create_piece(el, Coordinate(row, col), tm));
				}
			}
		}
	}
};
#endif
