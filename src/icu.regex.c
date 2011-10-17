
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <unicode/ustring.h>
#include <unicode/uregex.h>
#include "icu4lua.h"

// All icu.regex functions have these upvalues set
#define REGEX_UV_META			lua_upvalueindex(1)
#define REGEX_UV_USTRING_META	lua_upvalueindex(2)
#define REGEX_UV_USTRING_POOL	lua_upvalueindex(3)
#define REGEX_UV_TEXT			lua_upvalueindex(4)
#define REGEX_UV_GMATCH_AUX		lua_upvalueindex(5)
#define REGEX_UV_MATCH_META		lua_upvalueindex(6)

static int icu_regex_compile(lua_State *L) {
	const char* flagstring;
	uint32_t flags;
	UParseError pe;
	UErrorCode status;
	URegularExpression* new_regex;

	if (lua_isnumber(L,2)) {
		flags = (uint32_t)lua_tonumber(L,2);
	}
	else {
		flags = 0;
		if (flagstring = luaL_optstring(L,2,NULL)) {
			for (;flagstring[0];flagstring++) {
				switch(flagstring[0]) {
					case 'i':
						flags |= UREGEX_CASE_INSENSITIVE;
						break;
					case 'x':
						flags |= UREGEX_COMMENTS;
						break;
					case 's':
						flags |= UREGEX_DOTALL;
						break;
					case 'm':
						flags |= UREGEX_MULTILINE;
						break;
					case 'w':
						flags |= UREGEX_UWORD;
						break;
					default:
						return luaL_argerror(L,2,"unrecognised flag");
				}
			}
		}
	}
	status = U_ZERO_ERROR;
	if (lua_isstring(L,1)) {
		new_regex = uregex_openC(lua_tostring(L,1), flags, &pe, &status);
	}
	else {
		new_regex = uregex_open(
			icu4lua_checkustring(L,1,REGEX_UV_USTRING_META), (int32_t)icu4lua_ustrlen(L,1), flags, &pe, &status);
	}
	if (U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushnumber(L, pe.line);
		lua_pushnumber(L, pe.offset);
		// TODO: Add preContext and postContext
		return 4;
	}
	*(URegularExpression**)lua_newuserdata(L,sizeof(URegularExpression*)) = new_regex;
	lua_pushvalue(L, REGEX_UV_META);
	lua_setmetatable(L,-2);
	return 1;
}

static int icu_regex_lib__call(lua_State *L) {
	lua_remove(L, 1);
	return icu_regex_compile(L);
}

static int icu_regex_clone(lua_State *L) {
	UErrorCode status = U_ZERO_ERROR;
	URegularExpression* cloned_regex = uregex_clone(icu4lua_checkregex(L,1,REGEX_UV_META), &status);
	if (U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	*(URegularExpression**)lua_newuserdata(L, sizeof(URegularExpression*)) = cloned_regex;
	lua_pushvalue(L,REGEX_UV_META);
	lua_setmetatable(L,-2);
	return 1;
}

static void push_group(lua_State *L, URegularExpression* regex, int group_num) {
	UErrorCode status;
	UChar* group;
	int group_len;
	status = U_ZERO_ERROR;
	group_len = uregex_group(regex, group_num, NULL, 0, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		lua_error(L);
	}
	group = (UChar*)malloc(sizeof(UChar) * group_len);
	status = U_ZERO_ERROR;
	uregex_group(regex, group_num, group, group_len, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		free(group);
		lua_pushstring(L, u_errorName(status));
		lua_error(L);
	}
	icu4lua_pushustring(L, group, group_len, REGEX_UV_USTRING_META, REGEX_UV_USTRING_POOL);
	free(group);
}

static void add_group(luaL_Buffer *b, URegularExpression* regex, int group_num) {
	UErrorCode status;
	UChar* group;
	int group_len;
	status = U_ZERO_ERROR;
	group_len = uregex_group(regex, group_num, NULL, 0, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		if (status == U_INDEX_OUTOFBOUNDS_ERROR) {
			lua_pushstring(b->L, "invalid capture index");
		}
		else {
			lua_pushstring(b->L, u_errorName(status));
		}
		lua_error(b->L);
	}
	group = (UChar*)malloc(sizeof(UChar) * group_len);
	status = U_ZERO_ERROR;
	uregex_group(regex, group_num, group, group_len, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		free(group);
		lua_pushstring(b->L, u_errorName(status));
		lua_error(b->L);
	}
	icu4lua_addustring(b, group, group_len);
	free(group);
}

static void push_match(lua_State *L, URegularExpression* regex, UChar* text) {
	UErrorCode status;
	int group_count;
	int end;
	int i;

	status = U_ZERO_ERROR;
	group_count = uregex_groupCount(regex, &status);
	lua_createtable(L, group_count, 4);
	lua_pushliteral(L, "value");
	push_group(L, regex, 0);
	lua_rawset(L,-3);
	lua_pushliteral(L, "start");
	lua_pushnumber(L, uregex_start(regex, 0, &status) + 1);
	lua_rawset(L,-3);
	lua_pushliteral(L, "stop");
	lua_pushnumber(L, uregex_end(regex, 0, &status));
	lua_rawset(L,-3);
	for (i = 1; i <= group_count; i++) {
		if ((end = uregex_end(regex,i,&status)) == -1) {
			lua_pushboolean(L,0);
		}
		else {
			lua_createtable(L, 0, 3);
			lua_pushliteral(L, "start");
			lua_pushnumber(L, uregex_start(regex, i, &status) + 1);
			lua_rawset(L,-3);
			lua_pushliteral(L, "stop");
			lua_pushnumber(L, end);
			lua_rawset(L,-3);
			lua_pushliteral(L, "value");
			push_group(L, regex, i);
			lua_rawset(L,-3);
			lua_pushvalue(L, REGEX_UV_MATCH_META);
			lua_setmetatable(L, -2);
		}
		lua_rawseti(L,-2,i);
	}
	lua_pushvalue(L,-1);
	lua_rawseti(L,-2,0);
	lua_pushvalue(L, REGEX_UV_MATCH_META);
	lua_setmetatable(L, -2);
}

static int icu_regex_match(lua_State *L) {
	URegularExpression* regex;
	UErrorCode status;
	UBool success;

	icu4lua_checkregex(L,1,REGEX_UV_META);
	icu4lua_checkustring(L,2,REGEX_UV_USTRING_META);

	status = U_ZERO_ERROR;
	regex = uregex_clone(icu4lua_trustregex(L,1), &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	status = U_ZERO_ERROR;
	uregex_setText(regex, icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2), &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	if (lua_isnumber(L,3)) {
		status = U_ZERO_ERROR;
		uregex_setRegion(regex, (int32_t)lua_tonumber(L,3), (int32_t)icu4lua_ustrlen(L,2), &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
	}
	success = uregex_find(regex, -1, &status);
	if (U_FAILURE(status)) {
		uregex_close(regex);
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	if (success) {
		push_match(L, regex, NULL);
	}
	else {
		lua_pushboolean(L,0);
	}
	uregex_close(regex);
	return 1;
}

static int icu_regex__gc(lua_State *L);

static int gmatch_aux(lua_State *L) {
	URegularExpression* regex = icu4lua_trustregex(L,1);
	UErrorCode status = U_ZERO_ERROR;
	if (!uregex_findNext(regex, &status)) {
		// Force __gc cleanup now
		lua_pushnil(L);
		lua_setmetatable(L,1);
		icu_regex__gc(L);
		return 0;
	}
	push_match(L, regex, NULL);
	return 1;
}

static int icu_regex_gmatch(lua_State *L) {
	URegularExpression* regex;
	UErrorCode status;
	icu4lua_checkregex(L,1,REGEX_UV_META);
	icu4lua_checkustring(L,2,REGEX_UV_USTRING_META);
	lua_pushvalue(L, REGEX_UV_GMATCH_AUX);
	status = U_ZERO_ERROR;
	regex = uregex_clone(icu4lua_trustregex(L,1), &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	status = U_ZERO_ERROR;
	uregex_setText(regex, icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2), &status);
	if (U_FAILURE(status)) {
		uregex_close(regex);
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	*(URegularExpression**)lua_newuserdata(L, sizeof(URegularExpression*)) = regex;

	// Keep text ustring alive until __gc is called
	lua_pushvalue(L,-1);
	lua_pushvalue(L,2);
	lua_rawset(L,REGEX_UV_TEXT);

	// Set metatable on the regex, really just for __gc
	lua_pushvalue(L,REGEX_UV_META);
	lua_setmetatable(L,-2);

	return 2;
}

// Returns 0 if the callback appended its value to result_buffer and pushed no value, or 1 if it hasn't
// and has pushed either a ustring to be appended, or nil to keep what was there originally and not
// make a replacement.
typedef int ReplaceCallback(lua_State *L, URegularExpression* regex, luaL_Buffer* result_buffer);

static int rep_function(lua_State *L, URegularExpression* regex, luaL_Buffer* result_buffer) {
	lua_pushvalue(L, 3);
	push_match(L, regex, NULL);
	lua_call(L, 1, 1);
	return 1;
}

static int rep_lookup(lua_State *L, URegularExpression* regex, luaL_Buffer* result_buffer) {
	push_group(L, regex, 0);
	lua_gettable(L, 3);
	return 1;
}

static int rep_ustring(lua_State *L, URegularExpression* regex, luaL_Buffer* result_buffer) {
	lua_pushvalue(L, 3);
	return 1;
}

static int rep_ustring_with_captures(lua_State *L, URegularExpression* regex, luaL_Buffer* result_buffer) {
	UChar* replacement = icu4lua_trustustring(L,3);
	int32_t replacement_len = (int32_t)icu4lua_ustrlen(L,3);
	UChar* capture;
	int32_t chunk_len;
	for (;;) {
		int group;
		capture = u_memchr(replacement, '$', replacement_len);
		if (!capture) {
			break;
		}
		chunk_len = (int32_t)(capture - replacement);
		icu4lua_addustring(result_buffer, replacement, chunk_len);
		replacement_len -= (chunk_len+1);
		if (replacement_len == 0) {
			return 0;
		}
		replacement = (capture+1);
		if (!isdigit(replacement[0])) {
			icu4lua_addustring(result_buffer, replacement, 1);
			replacement_len--;
			replacement++;
			continue;
		}
		group = 0;
		while (isdigit(replacement[0])) {
			group = (group*10) + (replacement[0] - '0');
			replacement_len--;
			replacement++;
			if (replacement_len == 0) {
				break;
			}
		}
		add_group(result_buffer, regex, group);
	}
	if (replacement_len > 0) {
		icu4lua_addustring(result_buffer, replacement, replacement_len);
	}
	return 0;
}

static int icu_regex_replace(lua_State *L) {
	UErrorCode status;
	UBool success;
	URegularExpression* regex;
	UChar* text;
	int text_length;
	luaL_Buffer result_buffer;
	ReplaceCallback* replace_action = NULL;
	int start;

	regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	text = icu4lua_checkustring(L,2,REGEX_UV_USTRING_META);
	text_length = (int32_t)icu4lua_ustrlen(L,2);
	status = U_ZERO_ERROR;
	uregex_setText(regex, text, text_length, &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	if (lua_isfunction(L,3)) {
		replace_action = rep_function;
	}
	else if (lua_istable(L,3)) {
		replace_action = rep_lookup;
	}
	else {
		luaL_argcheck(L, lua_getmetatable(L,3) && lua_rawequal(L,-1,REGEX_UV_USTRING_META), 3, "expecting ustring/table/function");
		lua_pop(L,1);
		if (u_memchr(icu4lua_trustustring(L,3), '$', (int32_t)icu4lua_ustrlen(L,3))) {
			replace_action = rep_ustring_with_captures;
		}
		else {
			replace_action = rep_ustring;
		}
	}

	luaL_buffinit(L, &result_buffer);
	start = 0;
	for (;;) {
		int match_start;
		status = U_ZERO_ERROR;
		success = uregex_findNext(regex, &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		if (!success) {
			break;
		}
		status = U_ZERO_ERROR;
		match_start = uregex_start(regex, 0, &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		icu4lua_addustring(&result_buffer, text + start, match_start - start);
		status = U_ZERO_ERROR;
		start = uregex_end(regex, 0, &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		// Returns 1 if it pushed a value to the stack, 0 if it went directly to the result buffer
		if (replace_action(L, regex, &result_buffer)) {
			if (!lua_toboolean(L,-1)) {
				// nil/false - use the original value of the match
				icu4lua_addustring(&result_buffer, text + match_start, start - match_start);
			}
			else {
				if (!lua_getmetatable(L,-1) && lua_rawequal(L,-1,REGEX_UV_USTRING_META)) {
					return luaL_error(L, "replacement function/table must either yield a ustring or nil/false");
				}
				lua_pop(L,1);
				icu4lua_addustring(&result_buffer, icu4lua_trustustring(L,-1), icu4lua_ustrlen(L,-1));
			}
			lua_pop(L,1);
		}
	}
	if (start == 0) {
		lua_settop(L, 2);
		return 1;
	}
	else if (start < text_length) {
		icu4lua_addustring(&result_buffer, text + start, text_length - start);
	}

	icu4lua_pushuresult(&result_buffer, REGEX_UV_USTRING_META, REGEX_UV_USTRING_POOL);

	return 1;
}

static int icu_regex_split(lua_State *L) {
	URegularExpression* regex;
	const UChar* ustring;
	int32_t ustring_len;
	int limit;
	int start, end;
	int count;
	UErrorCode status;
	UBool success;
	regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	ustring = icu4lua_checkustring(L,2,REGEX_UV_USTRING_META);
	ustring_len = (int32_t)icu4lua_ustrlen(L,2);
	limit = luaL_optint(L,3,ustring_len+1);
	
	status = U_ZERO_ERROR;
	regex = uregex_clone(regex, &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	status = U_ZERO_ERROR;
	uregex_setText(regex, ustring, ustring_len, &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	lua_newtable(L);
	start = 0;
	count = 0;
	for (;;) {
		if (count == limit) {
			break;
		}
		status = U_ZERO_ERROR;
		success = uregex_findNext(regex, &status);
		if (U_FAILURE(status)) {
			uregex_close(regex);
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		if (!success) {
			break;
		}
		end = uregex_start(regex, 0, &status);
		icu4lua_pushustring(L, ustring + start, end - start, REGEX_UV_USTRING_META, REGEX_UV_USTRING_POOL);
		lua_rawseti(L,-2,++count);
		status = U_ZERO_ERROR;
		start = uregex_end(regex, 0, &status);
		if (U_FAILURE(status)) {
			uregex_close(regex);
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
	}
	uregex_close(regex);
	if (count == 0) {
		lua_pushvalue(L,2);
		lua_rawseti(L,-2,1);
		return 1;
	}
	icu4lua_pushustring(L, ustring + start, ustring_len - start, REGEX_UV_USTRING_META, REGEX_UV_USTRING_POOL);
	lua_rawseti(L,-2,++count);
	return 1;
}

static int icu_regex_text(lua_State *L) {
	URegularExpression* regex;
	const UChar* text;
	int32_t text_length;
	UErrorCode status;

	regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	
	if (lua_isnoneornil(L,2)) {
		// An unintentional consequence of having the REGEX_UV_TEXT table - it's really only there to
		// protect the text ustring from garbage collection. If it wasn't for that issue I'd probably
		// just use uregex_getText normally.
		lua_settop(L,1);
		lua_rawget(L,REGEX_UV_TEXT);
		return 1;
	}
	else {
		text = icu4lua_checkustring(L,2,REGEX_UV_USTRING_META);
		text_length = (int32_t)icu4lua_ustrlen(L,2);
		status = U_ZERO_ERROR;
		uregex_setText(regex, text, text_length, &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		// The text of a regex is not copied internally, so we must keep our ustring away from garbage collection
		// by putting it in a weak-keyed [regex -> text] table
		lua_pushvalue(L, 1);
		lua_pushvalue(L, 2);
		lua_rawset(L, REGEX_UV_TEXT);
		lua_settop(L,1);
		return 1;
	}
}

static int icu_regex_bounds(lua_State *L) {
	URegularExpression* regex;
	int32_t regionStart;
	int32_t regionEnd;
	UErrorCode status;

	regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	
	if (lua_isnoneornil(L,2) && lua_isnoneornil(L,3)) {
		status = U_ZERO_ERROR;
		regionStart = uregex_regionStart(regex, &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		lua_pushnumber(L, regionStart + 1);
		regionEnd = uregex_regionEnd(regex, &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		lua_pushnumber(L, regionEnd);
		return 2;
	}
	else {
		if (lua_isnoneornil(L,2)) {
			status = U_ZERO_ERROR;
			regionStart = uregex_regionStart(regex, &status);
			if (U_FAILURE(status)) {
				lua_pushstring(L, u_errorName(status));
				return lua_error(L);
			}
		}
		else if (!lua_isnumber(L,2)) {
			return luaL_argerror(L,2,"expecting number or nil");
		}
		else {
			regionStart = (int32_t)lua_tonumber(L,2) - 1;
		}
		if (lua_isnoneornil(L,3)) {
			status = U_ZERO_ERROR;
			regionEnd = uregex_regionEnd(regex, &status);
			if (U_FAILURE(status)) {
				lua_pushstring(L, u_errorName(status));
				return lua_error(L);
			}
		}
		else if (!lua_isnumber(L,3)) {
			return luaL_argerror(L,3,"expecting number or nil");
		}
		else {
			regionEnd = (int32_t)lua_tonumber(L,3);
		}
		status = U_ZERO_ERROR;
		uregex_setRegion(regex, regionStart, regionEnd, &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		lua_settop(L,1);
		return 1;
	}
}

static int icu_regex_transparentbounds(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	UErrorCode status;

	if (lua_gettop(L) > 1) {
		luaL_checktype(L,2,LUA_TBOOLEAN);
		status = U_ZERO_ERROR;
		uregex_useTransparentBounds(regex, lua_toboolean(L,2), &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		lua_settop(L,1);
		return 1;
	}

	status = U_ZERO_ERROR;
	lua_pushboolean(L, uregex_hasTransparentBounds(regex, &status));
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static int icu_regex_anchoringbounds(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	UErrorCode status;

	if (lua_gettop(L) > 1) {
		luaL_checktype(L,2,LUA_TBOOLEAN);
		status = U_ZERO_ERROR;
		uregex_useAnchoringBounds(regex, lua_toboolean(L,2), &status);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		lua_settop(L,1);
		return 1;
	}

	status = U_ZERO_ERROR;
	lua_pushboolean(L, uregex_hasAnchoringBounds(regex, &status));
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static int icu_regex_matches(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	UErrorCode status;

	if (lua_isnoneornil(L,2)) {
		status = U_ZERO_ERROR;
		lua_pushboolean(L, uregex_matches(regex, -1, &status));
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		return 1;
	}
	else if (!lua_isnumber(L,2)) {
		return luaL_argerror(L, 2, "expecting number or nil");
	}
	else {
		status = U_ZERO_ERROR;
		lua_pushboolean(L, uregex_matches(regex, (int32_t)lua_tonumber(L,2) - 1, &status));
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		return 1;
	}
}

static int icu_regex_lookingat(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	UErrorCode status;

	if (lua_isnoneornil(L,2)) {
		status = U_ZERO_ERROR;
		lua_pushboolean(L, uregex_lookingAt(regex, -1, &status));
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		return 1;
	}
	else if (!lua_isnumber(L,2)) {
		return luaL_argerror(L, 2, "expecting number or nil");
	}
	else {
		status = U_ZERO_ERROR;
		lua_pushboolean(L, uregex_lookingAt(regex, (int32_t)lua_tonumber(L,2) - 1, &status));
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		return 1;
	}
}

static int icu_regex_find(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	UErrorCode status;

	if (lua_isnoneornil(L,2)) {
		status = U_ZERO_ERROR;
		lua_pushboolean(L, uregex_findNext(regex, &status));
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		return 1;
	}
	else if (!lua_isnumber(L,2)) {
		return luaL_argerror(L, 2, "expecting number or nil");
	}
	else {
		status = U_ZERO_ERROR;
		lua_pushboolean(L, uregex_find(regex, (int32_t)lua_tonumber(L,2) - 1, &status));
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
		return 1;
	}
}

static int icu_regex_groupcount(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	UErrorCode status = U_ZERO_ERROR;
	lua_pushnumber(L, uregex_groupCount(regex, &status));
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static int icu_regex_reset(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	UErrorCode status = U_ZERO_ERROR;
	uregex_reset(regex, (int32_t)luaL_checknumber(L,2), &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 0;
}

static int icu_regex_value(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	UChar* group;
	int32_t group_num = luaL_optint(L,2,0);
	int32_t group_length;
	UErrorCode status;

	status = U_ZERO_ERROR;
	group_length = uregex_group(regex, group_num, NULL, 0, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	if (group_length < 0) {
		lua_pushnil(L);
		return 1;
	}
	group = (UChar*)malloc(sizeof(UChar) * group_length);
	status = U_ZERO_ERROR;
	uregex_group(regex, group_num, group, group_length, &status);
	if (U_FAILURE(status)) {
		free(group);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	icu4lua_pushustring(L, group, group_length, REGEX_UV_USTRING_META, REGEX_UV_USTRING_POOL);
	free(group);
	return 1;
}

static int icu_regex_range(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	int32_t groupNum = luaL_optint(L,2,0);
	UErrorCode status = U_ZERO_ERROR;
	lua_pushnumber(L, uregex_start(regex, groupNum, &status) + 1);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	status = U_ZERO_ERROR;
	lua_pushnumber(L, uregex_end(regex, groupNum, &status));
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 2;
}

static int icu_regex_decompile(lua_State *L) {
	URegularExpression* regex = icu4lua_checkregex(L,1,REGEX_UV_META);
	const UChar* pattern;
	int32_t patt_length;
	uint32_t flags;
	luaL_Buffer b;
	UErrorCode status = U_ZERO_ERROR;
	pattern = uregex_pattern(regex, &patt_length, &status);
	if (U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	icu4lua_pushustring(L, pattern, patt_length, REGEX_UV_USTRING_META, REGEX_UV_USTRING_POOL);

	status = U_ZERO_ERROR;
	flags = uregex_flags(regex, &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	if (flags & ~(UREGEX_CASE_INSENSITIVE | UREGEX_COMMENTS | UREGEX_DOTALL | UREGEX_MULTILINE | UREGEX_UWORD)) {
		lua_pushnumber(L, flags);
	}
	else {
		luaL_buffinit(L, &b);
		if (flags & UREGEX_CASE_INSENSITIVE)	luaL_addchar(&b, 'i');
		if (flags & UREGEX_COMMENTS)			luaL_addchar(&b, 'x');
		if (flags & UREGEX_DOTALL)				luaL_addchar(&b, 's');
		if (flags & UREGEX_MULTILINE)			luaL_addchar(&b, 'm');
		if (flags & UREGEX_UWORD)				luaL_addchar(&b, 'w');
		luaL_pushresult(&b);
	}

	return 2;
}

static int icu_regex_isregex(lua_State *L) {
	lua_pushboolean(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,REGEX_UV_META));
	return 1;
}

#define REGEX_SPECIAL "\\|*+?{}()[].^$"
// Using REGEX_SPECIAL, sizeof(REGEX_SPECIAL) will not work on all platforms
U_STRING_DECL(U_REGEX_SPECIAL, "\\|*+?{}()[].^$", 14);

static int icu_regex_escape(lua_State *L) {
	luaL_Buffer b;
	if (lua_isstring(L,1)) {
		const char* s = lua_tostring(L,1);
		const char* stop = s + (ptrdiff_t)lua_objlen(L,1);
		luaL_buffinit(L, &b);
		for (;s != stop; s++) {
			if (strchr(REGEX_SPECIAL, s[0])) {
				luaL_addchar(&b, '\\');
			}
			luaL_addchar(&b, s[0]);
		}
		luaL_pushresult(&b);
		return 1;
	}
	else {
		UChar* us;
		UChar* ustop;
		UChar u_backslash = '\\';
		int found = 0;
		luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,REGEX_UV_USTRING_META), 1, "expecting ustring or string");
		lua_pop(L,1);
		us = icu4lua_trustustring(L,1);
		ustop = us + icu4lua_ustrlen(L,1);
		luaL_buffinit(L, &b);
		for (;us != ustop; us++) {
			if (u_strchr(U_REGEX_SPECIAL, us[0])) {
				found = 1;
				icu4lua_addustring(&b, &u_backslash, 1);
			}
			icu4lua_addustring(&b, us, 1);
		}
		if (!found) {
			lua_settop(L,1);
			return 1;
		}
		icu4lua_pushuresult(&b, REGEX_UV_USTRING_META, REGEX_UV_USTRING_POOL);
		return 1;
	}
}

const static luaL_Reg icu_regex_lib[] = {

	// Creation
	{"compile", icu_regex_compile},
	{"__call", icu_regex_lib__call},
	{"clone", icu_regex_clone},

	// High-level functions
	{"match", icu_regex_match},
	{"gmatch", icu_regex_gmatch},
	{"replace", icu_regex_replace},
	{"split", icu_regex_split},

	// Setting up for atomic match
	{"text", icu_regex_text},
	{"bounds", icu_regex_bounds},
	{"transparentbounds", icu_regex_transparentbounds},
	{"anchoringbounds", icu_regex_anchoringbounds},

	// Atomic match operations
	{"matches", icu_regex_matches},
	{"lookingat", icu_regex_lookingat},
	{"find", icu_regex_find},
	{"reset", icu_regex_reset},

	// Successful atomic match info
	{"groupcount", icu_regex_value},
	{"value", icu_regex_value},
	{"range", icu_regex_range},

	// Misc
	{"decompile", icu_regex_decompile},
	{"isregex", icu_regex_isregex},
	{"escape", icu_regex_escape},

	{NULL, NULL}
};

typedef struct icuRegexConstant {
	const char* name;
	uint32_t value;
} icuRegexConstant;

const static icuRegexConstant icu_regex_constants[] = {
	{"CANON_EQ", UREGEX_CANON_EQ},
	{"CASE_INSENSITIVE", UREGEX_CASE_INSENSITIVE},
	{"COMMENTS", UREGEX_COMMENTS},
	{"DOTALL", UREGEX_DOTALL},
	{"LITERAL", UREGEX_LITERAL},
	{"MULTILINE", UREGEX_MULTILINE},
	{"UNIX_LINES", UREGEX_UNIX_LINES},
	{"UWORD", UREGEX_UWORD},
	{"ERROR_ON_UNKNOWN_ESCAPES", UREGEX_ERROR_ON_UNKNOWN_ESCAPES},
	{NULL, 0}
};

static int icu_regex__gc(lua_State *L) {
	lua_pushvalue(L,1);
	lua_pushnil(L);
	lua_rawset(L,REGEX_UV_TEXT);
	uregex_close(*(URegularExpression**)lua_touserdata(L,1));
	return 1;
}

static int icu_regex__tostring(lua_State *L) {
	lua_pushfstring(L, "icu.regex: %p", *(URegularExpression**)lua_touserdata(L,1));
	return 1;
}

static const luaL_Reg icu_regex_meta[] = {
	{"__gc", icu_regex__gc},
	{"__tostring", icu_regex__tostring},
	{NULL,NULL}
};

static icu_regex_match__tostring(lua_State *L) {
	lua_getfield(L, 1, "value");
	luaL_getmetafield(L, -1, "__tostring");
	lua_insert(L,-2);
	lua_call(L,1,1);
	lua_pushfstring(L, "match: \"%s\"", lua_tostring(L,-1));
	return 1;
}

static const luaL_Reg icu_regex_match_meta[] = {
	{"__tostring", icu_regex_match__tostring},
	{NULL,NULL}
};

int luaopen_icu_regex(lua_State *L) {
	int IDX_REGEX_META, IDX_USTRING_META, IDX_USTRING_POOL, IDX_REGEX_LIB, IDX_REGEX_TEXT, IDX_GMATCH_AUX, IDX_MATCH_META;
	luaL_Reg null_entry = {NULL,NULL};
	const icuRegexConstant* constant;
	const luaL_Reg* lib_entry;

	icu4lua_requireustringlib(L);

	luaL_newmetatable(L, "icu.regex");
	IDX_REGEX_META = lua_gettop(L);

	luaL_newmetatable(L, "icu.regex match");
	luaL_register(L, NULL, icu_regex_match_meta);
	IDX_MATCH_META = lua_gettop(L);

	icu4lua_pushustringmetatable(L);
	IDX_USTRING_META = lua_gettop(L);

	lua_newtable(L);
	IDX_REGEX_TEXT = lua_gettop(L);

	lua_pushvalue(L, IDX_REGEX_TEXT);
	lua_setfield(L, LUA_REGISTRYINDEX, "icu.regex text");

	lua_newtable(L);
	lua_pushliteral(L, "k");
	lua_setfield(L, -2, "__mode");
	lua_setmetatable(L, IDX_REGEX_TEXT);

	icu4lua_pushustringpool(L);
	IDX_USTRING_POOL = lua_gettop(L);
	
	luaL_register(L, "icu.regex", &null_entry);
	IDX_REGEX_LIB = lua_gettop(L);

	lua_pushvalue(L, IDX_REGEX_META);
	lua_pushvalue(L, IDX_USTRING_META);
	lua_pushvalue(L, IDX_USTRING_POOL);
	lua_pushvalue(L, IDX_REGEX_TEXT);
	lua_pushnil(L); // hopefully gmatch_aux won't need a reference to itself
	lua_pushvalue(L, IDX_MATCH_META);
	lua_pushcclosure(L, gmatch_aux, 6);
	IDX_GMATCH_AUX = lua_gettop(L);

	for (lib_entry = icu_regex_lib; lib_entry->name; lib_entry++) {
		lua_pushstring(L, lib_entry->name);
		lua_pushvalue(L, IDX_REGEX_META);
		lua_pushvalue(L, IDX_USTRING_META);
		lua_pushvalue(L, IDX_USTRING_POOL);
		lua_pushvalue(L, IDX_REGEX_TEXT);
		lua_pushvalue(L, IDX_GMATCH_AUX);
		lua_pushvalue(L, IDX_MATCH_META);
		lua_pushcclosure(L, lib_entry->func, 6);
		lua_rawset(L, IDX_REGEX_LIB);
	}
	for (constant = icu_regex_constants; constant->name; constant++) {
		lua_pushnumber(L, constant->value);
		lua_setfield(L, IDX_REGEX_LIB, constant->name);
	}
	for (lib_entry = icu_regex_meta; lib_entry->name; lib_entry++) {
		lua_pushstring(L, lib_entry->name);
		lua_pushvalue(L, IDX_REGEX_META);
		lua_pushvalue(L, IDX_USTRING_META);
		lua_pushvalue(L, IDX_USTRING_POOL);
		lua_pushvalue(L, IDX_REGEX_TEXT);
		lua_pushvalue(L, IDX_GMATCH_AUX);
		lua_pushvalue(L, IDX_MATCH_META);
		lua_pushcclosure(L, lib_entry->func, 6);
		lua_rawset(L, IDX_REGEX_META);
	}

	lua_pushvalue(L, IDX_REGEX_LIB);
	lua_setfield(L, IDX_REGEX_META, "__index");

	lua_pushvalue(L, IDX_REGEX_LIB);
	lua_setmetatable(L, IDX_REGEX_LIB);

	lua_settop(L, IDX_REGEX_LIB);
	return 1;
}
