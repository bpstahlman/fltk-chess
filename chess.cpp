#include <iostream>
#include <algorithm>
#include <string>
#include <regex>
#include <type_traits>
#include <fstream>
#include <unistd.h> // isatty
#include "chess_classes.h"

using namespace std;

Board B;

string text(Team t, bool cap = true) {
	return (t == Team::white ? (cap ? "White" : "white") : (cap ? "Black" : "black"));
}

// BPS: I notice that all callers iterate over all (or nearly all) piece types; is there a reason the iteration over
//      piece types isn't hidden within this function? I.e., is there a use case for calling search_path on a subset of
//      types?
Path Board::search_path(Path_type path_type, Coordinate search_from, Team mover_team, bool finding_threat, bool need_path) {
	int count;
	vector<Delta> vd{};
	vector<Delta> diagonals {{1,1},{1,-1},{-1,1},{-1,-1}};
	vector<Delta> non_diagonals {{1,0},{0,1},{-1,0},{0,-1}};
	switch (path_type) {
		case Path_type::Pawn:
			{
			int forward = (mover_team == Team::black ? 1 : -1);
			vd = {{forward,0},{forward*2,0},{forward,-1},{forward,1}};
			count = 1;
			break;
			}
		case Path_type::Knight:
			vd = {{2,1},{1,2},{-2,1},{-1,2},{2,-1},{1,-2},{-2,-1},{-1,-2}};
			count = 1;
			break;
		case Path_type::Bishop:
			vd = diagonals;
			count = 7;
			break;
		case Path_type::Rook:
			vd = non_diagonals;
			count = 7;
			break;
		case Path_type::Queen:
			vd = combine(diagonals, non_diagonals);
			count = 7;
			break;
		case Path_type::King:
			vd = combine(diagonals, non_diagonals);
			count = 1;
			break;
	}
	// BPS: Not sure it makes sense to pre-allocate these; allowing them to grow dynamically might be simpler, and even
	//      more efficient, since there would be no need to erase.
	vector<Coordinate> vr (vd.size(), search_from);
	vector<Path> paths (vd.size());
	bool remove = false;
	// BPS: Wouldn't things be simpler if these 2 nested loops were inverted? E.g., couldn't you just break out of the
	//      inner loop when you determine a direction doesn't work - without needing to "erase" anything? Also, you
	//      wouldn't need a vector of Path's, since you never return more than one Path anyways.
	for (int cnt = 0; cnt < count; ++cnt) {
		for (int idx = 0; idx < static_cast<int>(vd.size()); ++idx, remove = false) {
			vr[idx] += vd[idx];
			if (vr[idx].on_board()) {
				Piece* piece = get_at(vr[idx]);
				// BPS: Move is from perspective of opponent.
				// BPS: Feels as though the logic for the finding_threat case is very different from the
				//      non-finding_threat case.  Wondering whether all this should really be in one function...
				if (finding_threat) {
					if (need_path) {
						paths[idx].push_back(vr[idx]);
					}
					if (piece) {
						// BPS: We've encountered a piece, so movement along this path is terminated.
						//      if piece encountered is not threatening team's
						//      || (piece encountered is not type we're interested in
						//          && Rook/bishop not used as standin for Queen: i.e.
						//             piece encountered is not Queen || piece type of interest is something other than
						//             Rook or Bishop)
						if (piece->team != mover_team
							||
							(piece->type != path_type 
							 &&
							 ((path_type != Piece_type::Rook && path_type != Piece_type::Bishop) || piece->type != Piece_type::Queen))) {
							remove = true;
						}
						else {
							return (need_path ? paths[idx] : Path{vr[idx]});
						}
					}
				} else {
					// BPS: Sense of sought move is the opposite of finding_threat scenario: i.e., sought move is *from* search_from.
					if (piece && piece->team == mover_team) {
						// BPS: Can't move into square occupied by own piece.
						remove = true;
					} else {
						// BPS: See whether move to vr[idx] is valid, and if so, return the move.
						// BPS: Idea: One possibility for simplifying move/undo sequence would be to have Board maintain
						//      an undo "stack": e.g., when you tell it to move a piece, it saves the old state somehow,
						//      such that when you call undo, it can simply undo the last move. Just a thought... I
						//      could see move/undo being a useful primitive, especially if you eventually add some AI:
						//      might be nice to be able to hide the complexity a little better...
						pair<Piece*, bool> move_return = move(search_from, vr[idx]);
						Piece* moved_piece = get_at(vr[idx]);
						bool result_valid_return = moved_piece->result_valid(false);
						undo_move(search_from, vr[idx], move_return.first, move_return.second);
						if (result_valid_return) {
							return Path{vr[idx]};
						}
					}
				}
			} else {
				remove = true;
			}
			if (remove) {
				// BPS: Note: Erasing is inherently inefficient, since all subsequent elements need to copied down to
				//      heal the breach...
				// BPS: Erase the position corresponding to the direction that didn't pan out.
				vr.erase(vr.begin()+idx);
				// BPS: Erase the direction itself so we don't attempt to expand any further in it.
				vd.erase(vd.begin()+idx);
				// BPS: Erase the path vector corresponding to the defunct direction.
				paths.erase(paths.begin()+idx);
				// BPS: Make sure we don't skip the direction after the one we just removed.
				--idx;
			}
		}
	}
	return Path{};
}

// BPS: In the absence of a compelling reason to do otherwise, I'd pass a
//      collection like Path as const ref to avoid copy.
// BPS: This method should be defined in chess_classes.h or chess_classes.cpp.
bool Piece::path_valid(Path p) {
	auto p_b = p.begin(); // BPS: UNUSED!
	auto p_e = p.end();   // BPS: UNUSED!
	auto l = p.back();    // BPS: Possible to get a reference?
	Delta d = distance_to(l);
	Piece* dest_occ = B[l.row][l.col];
	// BPS: No need for explicit comparison with nullptr: p and !p are more idiomatic.
	// BPS: See note in Pawn::delta_valid regarding possible problem with this test.
	// BPS: Wondering whether things would be simplified by having delta_valid
	//      subsume the test for blocking opponents.
	if ((dest_occ == nullptr && any_of(barred_if_not_opp->begin(), barred_if_not_opp->end(), [&](Delta el) { return el == d; }))
	   ||
	   (dest_occ != nullptr && (dest_occ->team == team || any_of(barred_if_opp->begin(), barred_if_opp->end(), [&](Delta el) { return el == d; })))) {
		return false;
	}
	p.pop_back();
	for (auto coord : p) {
		Piece* occupant = B.get_at(coord);
		if (occupant != nullptr) {
			return false;
		}
	}
	return true;
}

// BPS: This method should be defined in chess_classes.h or chess_classes.cpp.
bool Piece::result_valid(bool modify_lane) {
	bool ret = false;
	// BPS: Get the piece in check lane adjacent to king.
	Piece* first_piece = (B.check_lane.size() > 0 ? B.get_at(B.check_lane.front()) : nullptr);
	// BPS: Upon entry to this function, check_lane still applies to move being validated; if the move is valid and
	//      modify_lane is set, it will be changed to reflect *opponent* before return.
	if (B.check_lane.empty()) {
		// BPS: Player wasn't already in check, but is its king threatened now?
		if (B.is_threatened(!team, B.get_king(team), false).empty()) {
			ret = true;
		}
		// BPS: Else player was in check...
		//      if king in check hasn't moved further into the check lane
		//      && (any of player's other pieces *has* moved into check lane
		//          || nothing in check lane square closest to king)
		//             Note: This test is wrong.
		//      && king is not threatened
	} else if ((not (B.check_lane.size() > 1 && first_piece && first_piece->type == Piece_type::King && first_piece->team == team))
			   &&
			   (any_of(B.check_lane.begin(), B.check_lane.end(), [&](Coordinate el) { return (B.get_at(el) && B.get_at(el)->team == team); }) 
			    ||
			    B.get_at(B.check_lane.front()) == nullptr)
		       &&
			   // BPS: Although this test would be sufficient on its own, the preceding (cheap) tests are tried first to
			   //      allow the more expensive test to be short-circuited.
		       B.is_threatened(!team, B.get_king(team), false).empty()) {
		ret = true;
	}
	// BPS: Do check test for opponent and cache for next call to this function.
	// BPS: Something a bit messy about the way check_lane is used here; wasn't clear to me at first that the assignment
	//      here was for other piece (next turn). Rather than assigning directly to a public member of board, perhaps
	//      you could pass a flag to is_threatened that told it whether it should cache the check_lane; in that case,
	//      some of the huge else if above might be consolidated into Board itself, which would then have all the
	//      information it needed to determine whether the threat represented by check_lane was ended.
	//      On further reflection, I'm not convinced that check_lane isn't a premature optimization.
	if (ret && modify_lane) {
		B.check_lane = B.is_threatened(team, B.get_king(!team), true);
	}
	return ret;
}

// BPS: Such a special-purpose function should probably have a less generic name. Also, it should probably be
// file-static to prevent namespace pollution.
int extract(int idx, smatch sm) {
	return stoi(sm[idx].str());
}

istream& my_getline(string& s)
{
retry:
	// If getline fails due to eof and cin isn't already connected to tty,
	// switch to it.
	if (!getline(cin, s) && !cin.bad() && cin.eof() && !isatty(STDIN_FILENO)) {
		static ifstream is;
		// Open stream on process' tty.
		is.open("/dev/tty", ios_base::in);
		// Redirect cin to the tty.
		cin.rdbuf(is.rdbuf());
		// getline should work now.
		goto retry;
	}
	// Idiosyncrasy: The stream we create sets eofbit but not failbit when
	// user hits Ctrl-D. Also, Ctrl-D has to be hit twice, and can't be the
	// first thing on the line...
	// TODO: Figure out why, but for now, just live with it...
	//if (cin.eof())
		//cin.setstate(ios_base::failbit);
	return cin;
}

int main() {
	// BPS: Might want to consider encapsulating game state somehow, as opposed to maintaining "turn" etc as local
	// variables in main().
	Team turn = Team::white;
	string user_input;
	regex rx("\\(([0-7]),([0-7])\\) -> \\(([0-7]),([0-7])\\)");
	smatch match;
	bool repeat = false;
	do {
		B.display();
		// BPS: If repeat were set false here, you could set it true if and only if there were a problem.
		do {
			if (repeat) {
				cout << "Invalid move. Still ";
			}
			cout << text(turn) << "'s turn:\n";
			if (!my_getline(user_input)) {
				cout << "Input stream terminated.";
				return 1;
			}
		} while (!regex_match(user_input, match, rx));
		Coordinate from {extract(1, match), extract(2, match)};
		Coordinate to {extract(3, match), extract(4, match)};
		Piece* piece = B.get_at(from);
		Piece* dest = B.get_at(to);
		//cout << "from = (" << from.row << "," << from.col << ")\n";
		// BPS: delta_valid() returns a valid path (else empty path) if move *geometry* is correct, ignoring any other
		//      pieces currently occupying squares in path.
		Path r_d_v = piece->delta_valid(to);
		// BPS: If we have a valid piece, it's that piece's turn, and the requested move is not blocked by the occupants
		//      (or lack thereof) of any squares in the path...
		if (piece && piece->team == turn && !r_d_v.empty() && piece->path_valid(r_d_v)) {
			// BPS: Consider having some sort of undo stack maintained by board, which would
			//      obviate the need for passing around so many piece objects
			//      and coordinates to different routines. The stack would be
			//      maintained by board, so you could just do...
			//      board.move(from, to);
			//      board.undo_move();
			pair<Piece*, bool> r_m = B.move(from, to);
			if (piece->result_valid(true)) {
				turn = !turn;
				repeat = false;
			} else {
				B.undo_move(from, to, r_m.first, r_m.second);
				repeat = true;
			}
		} else {
			repeat = true;
		}
		cout << "check_lane = {";
		for (el : B.check_lane) {
			cout << " {" << el.row << "," << el.col << "}";
		}
		cout << " }\n";
		// BPS: check_lane for the team we've just switched to was calculated and cached at the end of the
		//      result_valid() call that validated the move that just completed.
		if (B.check_lane.empty()) {
			// BPS: Player isn't in check, but we still need to check for stalemate.
			// BPS: Why is turn complemented here? Didn't the "turn = !turn" earlier already switch to the player for
			//      whom we need to test stalemate?
			if (B.stalemate_check(!turn)) {
				cout << "You've reached a stalemate. No one wins (or loses).";
				return 0;
			}
		} else {
			// BPS: Player is in check; make sure it's not checkmate.
			// BPS: See question at stalemate_check call above...
			// BPS: Checkmate test obviates need for stalemate testing.
			if (B.checkmate_check(!turn)) {
				cout << "Checkmate. " << text(turn) << " wins.";
				return 0;
			}
		}
	} while (true);
}

// vim:ts=4:sw=4:noet
