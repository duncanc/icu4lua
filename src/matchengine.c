
// This is a modified version of the matching code found in Lua's lstrlib.c
// adapted for use with ICU UCharIterators (and so can be used on both ustrings
// and utf-8 Lua strings)

#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <unicode/ustring.h>
#include <unicode/uchar.h>
#include "matchengine.h"

static int capture_to_close(UMatchState *ms) {
	int level = ms->level;
	for (level--; level>=0; level--) {
		if (ms->capture[level].what == CAP_UNFINISHED) {
			return level;
		}
	}
	return luaL_error(ms->L, "invalid pattern capture");
}

static int match(UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter);

static int start_capture(UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter, int what) {
	int level = ms->level;
	if (level >= LUA_MAXCAPTURES) {
		return luaL_error(ms->L, "too many captures");
	}
	ms->capture[level].start_state = pSourceIter->getState(pSourceIter);
	ms->capture[level].end_state = pSourceIter->getState(pSourceIter);
	ms->capture[level].what = what;
	ms->level = level+1;
	if (!match(ms, pPattIter, pSourceIter)) {
		ms->level--;
		return 0;
	}
	return 1;
}

static int end_capture(UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter) {
	int l = capture_to_close(ms);
	ms->capture[l].end_state = uiter_getState(pSourceIter);
	ms->capture[l].what = CAP_SUCCESSFUL;
	if (!match(ms, pPattIter, pSourceIter)) {
		ms->capture[l].what = CAP_UNFINISHED;
		return 0;
	}
	return 1;
}

static int32_t classend(UMatchState *ms, UCharIterator* pPattIter) {
	uint32_t restore_state = uiter_getState(pPattIter);
	uint32_t classend_state;
	UErrorCode status;
	switch(uiter_current32(pPattIter)) {
		case L_ESC:
			uiter_next32(pPattIter);
			if (uiter_current32(pPattIter) == '!') {
				uiter_next32(pPattIter);
				if (uiter_current32(pPattIter) == U_SENTINEL) {
					luaL_error(ms->L, "malformed pattern (ends with " LUA_QL("%%!") ")");
				}
			}
			else if (uiter_current32(pPattIter) == U_SENTINEL) {
				luaL_error(ms->L, "malformed pattern (ends with " LUA_QL("%%") ")");
			}
			uiter_next32(pPattIter);
			break;
		case '[':
			uiter_next32(pPattIter);
			if (uiter_current32(pPattIter) == '^') {
				uiter_next32(pPattIter);
			}
			for (;;) {
				switch(uiter_current32(pPattIter)) {
					case ']':
						uiter_next32(pPattIter);
						break;
					case U_SENTINEL:
						luaL_error(ms->L, "malformed pattern (missing " LUA_QL("]") ")");
					case L_ESC: // skip escapes (i.e. %])
						uiter_next32(pPattIter);
						uiter_next32(pPattIter);
						continue;
					default:
						uiter_next32(pPattIter);
						continue;
				}
				break;
			}
			break;
		default:
			uiter_next32(pPattIter);
			break;
	}
	classend_state = uiter_getState(pPattIter);
	status = U_ZERO_ERROR;
	uiter_setState(pPattIter, restore_state, &status);
	if (U_FAILURE(status)) {
		luaL_error(ms->L, "unable to set state after classend: %s", u_errorName(status));
	}
	return classend_state;
}

static int match_class (UChar32 c, UChar32 cl) {
	int res;
	switch (tolower(cl)) {
		case 'a':
			res = isalpha(c);
			break;
		case 'c':
			res = iscntrl(c);
			break;
		case 'd':
			res = isdigit(c);
			break;
		case 'l':
			res = islower(c);
			break;
		case 'p':
			res = ispunct(c);
			break;
		case 's':
			res = isspace(c);
			break;
		case 'u':
			res = isupper(c);
			break;
		case 'w':
			res = isalnum(c);
			break;
		case 'x':
			res = isxdigit(c);
			break;
		case 'z':
			res = (c == 0);
			break;
		default:
			return (cl == c);
	}
	return (islower(cl) ? res : !res);
}

static int match_class_u (UChar32 c, UChar32 cl) {
	int res;
	switch (tolower(cl)) {
		case 'a':
			res = u_isalpha(c);
			break;
		case 'c':
			res = u_iscntrl(c);
			break;
		case 'd':
			res = u_isdigit(c);
			break;
		case 'l':
			res = u_islower(c);
			break;
		case 'p':
			res = u_ispunct(c);
			break;
		case 's':
			res = u_isspace(c);
			break;
		case 'u':
			res = u_isupper(c);
			break;
		case 'w':
			res = u_isalnum(c);
			break;
		case 'x':
			res = u_isxdigit(c);
			break;
		case 'z':
			res = (c == 0);
			break;
		default:
			return (cl == c);
	}
	return (islower(cl) ? res : !res);
}

static int matchbracketclass(UChar32 sc, UCharIterator* pPattIter, uint32_t endclass_state) {
	uint32_t restore_state = uiter_getState(pPattIter);
	UChar32 pc; // Pattern character
	UChar32 rc; // End-range character
	int sig = 1;
	UErrorCode status;
	uiter_next32(pPattIter);
	if (uiter_current32(pPattIter) == '^') {
		sig = 0;
		uiter_next32(pPattIter); // skip the ^
	}
	for (;;) {
		switch(pc = uiter_current32(pPattIter)) {
			case L_ESC:
				uiter_next32(pPattIter);
				pc = uiter_current32(pPattIter);
				if (pc == '!') {
					uiter_next32(pPattIter);
					pc = uiter_current32(pPattIter);
					if (match_class_u(sc, pc)) {
						status = U_ZERO_ERROR;
						uiter_setState(pPattIter, restore_state, &status);
						return sig;
					}
				}
				else if (match_class(sc, pc)) {
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, restore_state, &status);
					return sig;
				}
				uiter_next32(pPattIter);
			case ']':
				status = U_ZERO_ERROR;
				uiter_setState(pPattIter, restore_state, &status);
				return !sig;
			default:
				if (pc == sc) {
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, restore_state, &status);
					return sig;
				}
				uiter_next32(pPattIter);
				if (uiter_current32(pPattIter) == '-') {
					uiter_next32(pPattIter);
					rc = uiter_current32(pPattIter);
					if (rc != ']') {
						if (sc > pc && sc <= rc) {
							status = U_ZERO_ERROR;
							uiter_setState(pPattIter, restore_state, &status);
							return sig;
						}
						uiter_next32(pPattIter);
					}
				}
		}
	}
}

static int singlematch(int uc, UCharIterator* pPattIter, uint32_t classend_state) {
	switch(uiter_current32(pPattIter)) {
		case '.':
			return 1;  // matches any char
		case L_ESC: {
			UChar32 cl;
			uiter_next32(pPattIter);
			cl = uiter_current32(pPattIter);
			if (cl == '!') {
				uiter_next32(pPattIter);
				cl = uiter_current32(pPattIter);
				uiter_previous32(pPattIter);
				uiter_previous32(pPattIter);
				return match_class_u(uc, cl);
			}
			else {
				uiter_previous32(pPattIter);
				return match_class(uc, cl);
			}
		}
		case '[':
			return matchbracketclass(uc, pPattIter, classend_state);
		default:
			return (uiter_current32(pPattIter) == uc);
	}
}

static int matchbalance (UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter) {
	UChar32 open, close;
	open = uiter_current32(pPattIter);
	uiter_next32(pPattIter);
	if ((close = uiter_current32(pPattIter)) == U_SENTINEL) {
		luaL_error(ms->L, "unbalanced pattern");
	}
	uiter_next32(pPattIter);
	if (uiter_current32(pSourceIter) != open) {
		return 0;
	}
	else {
		int cont = 1;
		uint32_t restore_state = uiter_getState(pSourceIter);
		UErrorCode status;
		for (uiter_next32(pSourceIter); uiter_current32(pSourceIter) != U_SENTINEL; uiter_next32(pSourceIter)) {
			if (uiter_current32(pSourceIter) == close) {
				if (--cont == 0) {
					uiter_next32(pSourceIter);
					return 1;
				}
			}
			else if (uiter_current32(pSourceIter) == open) {
				cont++;
			}
		}
		status = U_ZERO_ERROR;
		uiter_setState(pSourceIter, restore_state, &status);
	}
	return 0; // string ends out of balance
}

static int check_capture(UMatchState *ms, int l) {
	l -= '1';
	if (l < 0 || l >= ms->level || ms->capture[l].what == CAP_UNFINISHED)
		return luaL_error(ms->L, "invalid capture index");
	return l;
}

// TODO: Find a better way to do this?
static int match_capture(UMatchState *ms, UCharIterator* pSourceIter, int l) {
	uint32_t cap_state, cap_end_state;
	uint32_t match_state;
	UChar32 cap_char;
	UErrorCode status;
	l = check_capture(ms, l);
	if (ms->capture[l].what == CAP_POSITION) {
		return 0;
	}
	match_state = uiter_getState(pSourceIter);
	cap_state = ms->capture[l].start_state;
	cap_end_state = ms->capture[l].end_state;
	for (;;) {
		if (cap_state == cap_end_state) {
			status = U_ZERO_ERROR;
			uiter_setState(pSourceIter, match_state, &status);
			return 1;
		}
		status = U_ZERO_ERROR;
		uiter_setState(pSourceIter, cap_state, &status);
		cap_char = uiter_current32(pSourceIter);
		uiter_next32(pSourceIter);
		cap_state = uiter_getState(pSourceIter);
		status = U_ZERO_ERROR;
		uiter_setState(pSourceIter, match_state, &status);
		if (uiter_current32(pSourceIter) != cap_char) {
			return 0;
		}
		uiter_next32(pSourceIter);
		match_state = uiter_getState(pSourceIter);
	}
}


// Try to match the current pattern as many times as possible, then try to match the next pattern
// immediately after the end of the run - if it does not match, backtrack over the current pattern
// matches until we find an "overlapping" match there, or we fail by reaching the beginning again.
static int max_expand(UMatchState *ms, UCharIterator* pPattIter, UCharIterator* pSourceIter, uint32_t afterclass_state) {
	UErrorCode status;
	uint32_t patt_restore_state, string_restore_state;
	int i = 0;
	for (;;) {
		if (uiter_current32(pSourceIter) == U_SENTINEL || !singlematch(uiter_current32(pSourceIter), pPattIter, afterclass_state)) {
			break;
		}
		uiter_next32(pSourceIter);
		i++;
	}
	status = U_ZERO_ERROR;
	uiter_setState(pPattIter, afterclass_state, &status);
	if (U_FAILURE(status)) {
		luaL_error(ms->L, "cannot restore state in max_expand: %s", u_errorName(status));
	}
	uiter_next32(pPattIter);
	patt_restore_state = uiter_getState(pPattIter);
	string_restore_state = uiter_getState(pSourceIter);
	while (i >= 0) {
		if (match(ms, pPattIter, pSourceIter)) {
			return 1;
		}
		i--;
		status = U_ZERO_ERROR;
		uiter_setState(pPattIter, patt_restore_state, &status);
		status = U_ZERO_ERROR;
		uiter_setState(pSourceIter, string_restore_state, &status);
		uiter_previous32(pSourceIter);
		string_restore_state = uiter_getState(pSourceIter);
	}
	return 0;
}

static int min_expand(UMatchState *ms, UCharIterator* pPattIter, UCharIterator* pSourceIter, uint32_t end_state) {
	uint32_t start_state = uiter_getState(pPattIter);
	UErrorCode status;
	for (;;) {
		// First set the iterator to the pattern part *after* the minimally-expanded part
		status = U_ZERO_ERROR;
		uiter_setState(pPattIter, end_state, &status);
		uiter_next32(pPattIter);
		// If this matches, report success here
		if (match(ms, pPattIter, pSourceIter)) {
			return 1;
		}
		// Now set the iterator back to the minimally-expanded part
		status = U_ZERO_ERROR;
		uiter_setState(pPattIter, start_state, &status);
		// If this fails, then neither part matches and we fail here
		if (uiter_current32(pSourceIter)==U_SENTINEL || !singlematch(uiter_current32(pSourceIter), pPattIter, end_state)) {
			return 0;
		}
		uiter_next32(pSourceIter);  // try with one more repetition
	}
}

static int match(UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter) {
	UErrorCode status;
	init: // using goto to optimize tail recursion
	switch(uiter_current32(pPattIter)) {
		case '(': // start capture
			uiter_next32(pPattIter);
			if (uiter_current32(pPattIter) == ')') { // position capture?
				uiter_next32(pPattIter);
				return start_capture(ms, pPattIter, pSourceIter, CAP_POSITION);
			}
			return start_capture(ms, pPattIter, pSourceIter, CAP_UNFINISHED);
		case ')': // end capture
			uiter_next32(pPattIter);
			return end_capture(ms, pPattIter, pSourceIter);
		case L_ESC:
			uiter_next32(pPattIter);
			switch(uiter_current32(pPattIter)) {
				case 'b': // balanced string?
					uiter_next32(pPattIter);
					if (!matchbalance(ms, pPattIter, pSourceIter)) {
						return 0;
					}
					goto init;
				case 'f': { // frontier?
					uint32_t startclass_state, classend_state;
					UChar32 previous;
					uiter_next32(pPattIter);
					if (uiter_current32(pPattIter) != '[') {
						luaL_error(ms->L, "missing " LUA_QL("[") " after " LUA_QL("%%f") " in pattern");
					}
					startclass_state = uiter_getState(pPattIter);
					classend_state = classend(ms, pPattIter); // points to what is next
					if (pSourceIter->hasPrevious(pSourceIter)) {
						uiter_previous32(pSourceIter);
						previous = uiter_current32(pSourceIter);
						uiter_next32(pSourceIter);
					}
					else {
						previous = '\0';
					}
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, startclass_state, &status);
					if (matchbracketclass(previous, pPattIter, classend_state)) {
						return 0;
					}
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, startclass_state, &status);
					if (!matchbracketclass(uiter_current32(pSourceIter), pPattIter, classend_state)) {
						return 0;
					}
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, classend_state, &status);
					goto init;
				}
				default:
					if (isdigit(uiter_current32(pPattIter))) { // capture results %0 to %9?
						if (!match_capture(ms, pSourceIter, uiter_current32(pPattIter))) {
							return 0;
						}
						uiter_next32(pPattIter);
						goto init;
					}
					uiter_previous32(pPattIter);
					goto dflt; // case default
			}
		case U_SENTINEL: // end of pattern
			return 1; // match succeeded
		case '$':
			if (uiter_current32(pSourceIter) == U_SENTINEL) {
				return 1;
			}
			goto dflt;
		default: dflt: { // it is a pattern item
			int m;
			uint32_t startclass_state = uiter_getState(pPattIter);
			uint32_t afterclass_state = classend(ms, pPattIter);
			status = U_ZERO_ERROR;
			uiter_setState(pPattIter, startclass_state, &status);
			if (U_FAILURE(status)) {
				luaL_error(ms->L, "cannot restore state to beginning of match class");
			}
			m = uiter_current32(pSourceIter)!=U_SENTINEL && singlematch(uiter_current32(pSourceIter), pPattIter, afterclass_state);
			status = U_ZERO_ERROR;
			uiter_setState(pPattIter, afterclass_state, &status);
			switch (uiter_current32(pPattIter)) {
				case '?': // optional
					uiter_next32(pPattIter);
					if (m) {
						uiter_next32(pSourceIter);
						if (match(ms, pPattIter, pSourceIter)) {
							return 1;
						}
						status = U_ZERO_ERROR;
						uiter_setState(pPattIter, afterclass_state, &status);
						uiter_next32(pPattIter);
					}
					goto init;
				case '*': // 0 or more repetitions
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, startclass_state, &status);
					return max_expand(ms, pPattIter, pSourceIter, afterclass_state);
				case '+': // 1 or more repetitions
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, startclass_state, &status);
					return m ? max_expand(ms, pPattIter, pSourceIter, afterclass_state) : 0;
				case '-': // 0 or more repetitions (minimum)
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, startclass_state, &status);
					return min_expand(ms, pPattIter, pSourceIter, afterclass_state);
				default:
					if (!m) {
						status = U_ZERO_ERROR;
						uiter_setState(pPattIter, startclass_state, &status);
						return 0;
					}
					uiter_next32(pSourceIter);
					status = U_ZERO_ERROR;
					uiter_setState(pPattIter, afterclass_state, &status);
					goto init;
			}
		}
	}
}

int uiter_match_aux(UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter) {
	uint32_t pattState;
	uint32_t stringState;
	UErrorCode status;
	int anchor = (uiter_current32(pPattIter) == '^') ? (uiter_next32(pPattIter), 1) : 0;

	for (;;) {
		pattState = uiter_getState(pPattIter);
		stringState = uiter_getState(pSourceIter);

		ms->level = 0;
		ms->capture[0].start_state = stringState;
		if (match(ms, pPattIter, pSourceIter)) {
			return 1;
		}

		status = U_ZERO_ERROR;
		uiter_setState(pPattIter, pattState, &status);
		if (U_FAILURE(status)) {
			return luaL_error(ms->L, "cannot restore pattern iterator state: %s", u_errorName(status));
		}
		status = U_ZERO_ERROR;
		uiter_setState(pSourceIter, stringState, &status);
		if (U_FAILURE(status)) {
			return luaL_error(ms->L, "cannot restore string iterator state: %s", u_errorName(status));
		}

		if (anchor) {
			break;
		}
		if (uiter_current32(pSourceIter) == U_SENTINEL) {
			break;
		}
		uiter_next32(pSourceIter);
	}
	return 0;
}
static int push_captures(UMatchState* ms, UCharIterator* pSourceIter) {
	UErrorCode status;
	int i;
	if (ms->level == 0) {
		ms->pushRange(ms, ms->capture[0].start_state, ms->end_state);
		return 1;
	}
	for (i = 0; i < ms->level; i++) {
		switch(ms->capture[i].what) {
			case CAP_POSITION:
				status = U_ZERO_ERROR;
				pSourceIter->setState(pSourceIter, ms->capture[i].start_state, &status);
				lua_pushinteger(ms->L, pSourceIter->getIndex(pSourceIter, UITER_CURRENT) + 1);
				break;
			case CAP_SUCCESSFUL: {
				ms->pushRange(ms, ms->capture[i].start_state, ms->capture[i].end_state);
				break;
			}
			default:
				return luaL_error(ms->L, "unfinished captures in pattern");
		}
	}
	return ms->level;
}

int iter_match(UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter, int init, int find) {
	UErrorCode status;
	if (init > 0) {
		pSourceIter->move(pSourceIter, init-1, UITER_ZERO);
	}
	else if (init < 0) {
		pSourceIter->move(pSourceIter, init, UITER_LIMIT);
	}
	if (!uiter_match_aux(ms, pPattIter, pSourceIter)) {
		lua_pushnil(ms->L);
		return 1;
	}
	ms->end_state = uiter_getState(pSourceIter);
	if (find) {
		lua_pushinteger(ms->L, pSourceIter->getIndex(pSourceIter, UITER_CURRENT));
		status = U_ZERO_ERROR;
		uiter_setState(pSourceIter, ms->capture[0].start_state, &status);
		lua_pushinteger(ms->L, 1 + pSourceIter->getIndex(pSourceIter, UITER_CURRENT));
		lua_insert(ms->L, -2);
		if (ms->level == 0) {
			return 2;
		}
		return push_captures(ms, pSourceIter) + 2;
	}
	else {
		return push_captures(ms, pSourceIter);
	}
}

int uiter_gsub_aux(UMatchState* ms, UCharIterator* pPattIter, UCharIterator *pSourceIter,
				   ProcessUMatchStateFunc on_match, int max_replacements) {
	int replacements = 0;
	uint32_t source_state = uiter_getState(pSourceIter);
	UErrorCode status;
	int anchor = (uiter_current32(pPattIter) == '^') ? (uiter_next32(pPattIter), 1) : 0;
	while (replacements < max_replacements) {
		ms->level = 0;
		ms->capture[0].start_state = uiter_getState(pSourceIter);
		if (match(ms, pPattIter, pSourceIter)) {
			ms->end_state = pSourceIter->getState(pSourceIter);
			if (ms->level == 0) {
				ms->level = 1;
				ms->capture[0].what = CAP_SUCCESSFUL;
				ms->capture[0].end_state = ms->end_state;
			}
			// add the replacement to output
			on_match(ms);

			// set the source_state to after the match
			source_state = pSourceIter->getState(pSourceIter);
			replacements++;
			if (ms->capture[0].start_state == ms->end_state) {
				if (uiter_current32(pSourceIter) == U_SENTINEL) {
					break;
				}
				else {
					uint32_t old_state = source_state;
					uiter_next32(pSourceIter);
					source_state = uiter_getState(pSourceIter);
					ms->addRange(ms, old_state, source_state);
				}
			}
		}
		else {
			// add the single non-matching character to output, set the source_state to the next character
			uint32_t old_state = source_state;
			status = U_ZERO_ERROR;
			pSourceIter->setState(pSourceIter, source_state, &status);
			if (pSourceIter->current(pSourceIter) == U_SENTINEL) {
				break;
			}
			pSourceIter->next(pSourceIter);
			source_state = pSourceIter->getState(pSourceIter);
			ms->addRange(ms, old_state, source_state);
		}
		if (anchor) {
			break;
		}
		pPattIter->move(pPattIter, 0, UITER_ZERO);
	}
	source_state = uiter_getState(pSourceIter);
	pSourceIter->move(pSourceIter, 0, UITER_LIMIT);
	ms->addRange(ms, source_state, uiter_getState(pSourceIter));
	return replacements;
}

int gmatch_aux (lua_State *L) {
	UErrorCode status;
	GmatchState* gms = (GmatchState*)lua_touserdata(L, lua_upvalueindex(3));
	gms->ms.L = L;
	status = U_ZERO_ERROR;
	uiter_setState(&gms->sourceIter, gms->source_state, &status);
	for (;;) {
		gms->ms.level = 0;
		gms->ms.capture[0].start_state = gms->source_state;
		gms->pattIter.move(&gms->pattIter, 0, UITER_ZERO);
		gms->ms.capture[0].start_state = uiter_getState(&gms->sourceIter);
		if (match(&gms->ms, &gms->pattIter, &gms->sourceIter)) {
			gms->ms.end_state = uiter_getState(&gms->sourceIter);
			gms->source_state = uiter_getState(&gms->sourceIter);
			if (gms->source_state == gms->ms.capture[0].start_state) {
				if (!gms->sourceIter.hasNext(&gms->sourceIter)) {
					if (gms->matched_empty_end) {
						return 0;
					}
					else {
						gms->matched_empty_end = 1;
					}
				}
				uiter_next32(&gms->sourceIter);
				gms->source_state = uiter_getState(&gms->sourceIter);
			}
			return push_captures(&gms->ms, &gms->sourceIter);
		}
		status = U_ZERO_ERROR;
		uiter_setState(&gms->sourceIter, gms->source_state, &status);
		if (uiter_current32(&gms->sourceIter) == U_SENTINEL) {
			break;
		}
		uiter_next32(&gms->sourceIter);
		gms->source_state = uiter_getState(&gms->sourceIter);
	}
	return 0; // end of gmatch
}
