
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>
#include <unicode/uchar.h>
#include <unicode/ustdio.h>
#include "icu4lua.h"
#include "matchengine.h"
#include "formatting.h"

// All icu.ustring functions have these upvalues set
#define USTRING_UV_META		lua_upvalueindex(1)
#define USTRING_UV_POOL		lua_upvalueindex(2)

static int icu_ustring_decode(lua_State *L) {
	size_t byte_length;
	const char* source = luaL_checklstring(L,1,&byte_length);
	const char* source_limit = source + byte_length;
    UErrorCode status = U_ZERO_ERROR;
    UConverter* conv;
	luaL_Buffer build_buffer;
	UChar* temp_buffer;
	UChar* target;
	UChar* target_limit;
	conv = ucnv_open(luaL_optstring(L,2,"utf-8"), &status);
	if (U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	luaL_buffinit(L, &build_buffer);
	temp_buffer = icu4lua_prepubuffer(&build_buffer);
	target = temp_buffer;
	target_limit = target + ICU4LUA_UBUFFERSIZE;
	for (;;) {
		status = U_ZERO_ERROR;
		ucnv_toUnicode(conv, &target, target_limit, &source, source_limit, NULL, TRUE, &status);
		switch(status) {
			case U_ZERO_ERROR:
				icu4lua_addusize(&build_buffer, target - temp_buffer);
				ucnv_close(conv);
				icu4lua_pushuresult(&build_buffer, USTRING_UV_META, USTRING_UV_POOL);
				return 1;
			case U_BUFFER_OVERFLOW_ERROR:
				icu4lua_addusize(&build_buffer, target - temp_buffer);
				temp_buffer = icu4lua_prepubuffer(&build_buffer);
				target = temp_buffer;
				target_limit = target + ICU4LUA_UBUFFERSIZE;
				break;
			default:
				ucnv_close(conv);
				lua_pushnil(L);
				lua_pushstring(L, u_errorName(status));
				return 2;
		}
	}
}

static int icu_ustring_unescape(lua_State *L) {
	UChar* new_ustring;
	int32_t uchar_len;

	uchar_len = u_unescape(luaL_checkstring(L,1), NULL, 0);
	new_ustring = (UChar*)malloc(sizeof(UChar) * uchar_len);
	u_unescape(lua_tostring(L,1), new_ustring, uchar_len);
	icu4lua_pushustring(L, new_ustring, uchar_len, USTRING_UV_META, USTRING_UV_POOL);
	free(new_ustring);
	return 1;
}

static int icu_ustring_encode(lua_State *L) {
	UErrorCode status;
	UConverter* conv;
	luaL_Buffer build_buffer;
	char* temp_buffer;
	char* target;
	char* target_limit;
	UChar* source = icu4lua_checkustring(L,1,USTRING_UV_META);
	UChar* source_limit = source + icu4lua_ustrlen(L,1);

	lua_settop(L,2);

	// Initialise the converter and string building buffer
	status = U_ZERO_ERROR;
	conv = ucnv_open(luaL_optstring(L,2,"utf-8"), &status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	luaL_buffinit(L, &build_buffer);

	// Get the target and target limit - temp_buffer must be freed before we return
	temp_buffer = luaL_prepbuffer(&build_buffer);
	target = temp_buffer;
	target_limit = temp_buffer + LUAL_BUFFERSIZE;

	for (;;) {
		status = U_ZERO_ERROR;
		ucnv_fromUnicode(conv, &target, target_limit, &source, source_limit, NULL, TRUE, &status);
		switch(status) {
			case U_BUFFER_OVERFLOW_ERROR:
				luaL_addsize(&build_buffer, (const char*)target - (const char*)temp_buffer);
				temp_buffer = luaL_prepbuffer(&build_buffer);
				target = temp_buffer;
				target_limit = temp_buffer + LUAL_BUFFERSIZE;
				break;
			case U_ZERO_ERROR:
				luaL_addsize(&build_buffer, (const char*)target - (const char*)temp_buffer);
				ucnv_close(conv);
				luaL_pushresult(&build_buffer);
				return 1;
			default:
				ucnv_close(conv);
				lua_pushstring(L, u_errorName(status));
				return lua_error(L);
		}
	}
}

static int icu_ustring_len(lua_State *L) {
	lua_pushinteger(L, u_countChar32(icu4lua_checkustring(L,1,USTRING_UV_META), (int32_t)icu4lua_ustrlen(L,1)));
	return 1;
}

static int icu_ustring_rep(lua_State *L) {
	int reps;
	luaL_Buffer concat_buffer;

	icu4lua_checkustring(L,1,USTRING_UV_META);
	reps = luaL_checkint(L,2);
	luaL_buffinit(L, &concat_buffer);
	for (; reps > 0; reps--) {
		luaL_addlstring(&concat_buffer, lua_touserdata(L,1), lua_objlen(L,1));
	}
	icu4lua_pushuresult(&concat_buffer, USTRING_UV_META, USTRING_UV_POOL);
	return 1;
}

static int icu_ustring_sub(lua_State *L) {
	const UChar* ustring = icu4lua_checkustring(L,1,USTRING_UV_META);
    UCharIterator iter;
    int start_pos;
    int end_pos;
    int i;
    uint32_t start_ucharpos;
    uint32_t end_ucharpos;

    start_pos = luaL_optint(L,2,1);
    end_pos = luaL_optint(L,3,-1);

    lua_settop(L,1);
    uiter_setString(&iter, ustring, (int32_t)icu4lua_ustrlen(L,1));

    if (end_pos == 0) {
        lua_pushliteral(L, "");
		icu4lua_internrawustring(L, USTRING_UV_META, USTRING_UV_POOL);
        return 1;
    }
    else if (end_pos < 0) {
        iter.move(&iter, 0, UITER_LIMIT);
        for (i = -1; i > end_pos; i--) {
            if (!iter.hasPrevious(&iter)) {
                lua_pushliteral(L, "");
				icu4lua_internrawustring(L, USTRING_UV_META, USTRING_UV_POOL);
                return 1;
            }
            iter.move(&iter, -1, UITER_CURRENT);
        }
    }
    else {
        iter.move(&iter, end_pos, UITER_START);
    }
    end_ucharpos = iter.getState(&iter);

    if (start_pos < 0) {
        iter.move(&iter, start_pos, UITER_LIMIT);
    }
    else {
        iter.move(&iter, 0, UITER_START);
        for (i = 1; i < start_pos; i++) {
            if (!iter.hasNext(&iter)) {
				lua_pushliteral(L,"");
				icu4lua_internrawustring(L, USTRING_UV_META, USTRING_UV_POOL);
                return 1;
            }
            iter.move(&iter, 1, UITER_CURRENT);
        }
    }
    start_ucharpos = iter.getState(&iter);

	if (start_ucharpos > end_ucharpos) {
		lua_pushliteral(L,"");
		icu4lua_internrawustring(L, USTRING_UV_META, USTRING_UV_POOL);
	}
	else {
		icu4lua_pushustring(L,
			ustring + start_ucharpos, end_ucharpos - start_ucharpos,
			USTRING_UV_META, USTRING_UV_POOL);
	}
    return 1;
}

static int icu_ustring_reverse(lua_State *L) {
	UChar* ustring = icu4lua_checkustring(L,1,USTRING_UV_META);
	int32_t uchar_len = (int32_t)icu4lua_ustrlen(L,1);
	UChar* new_ustring;
	UCharIterator iter;
	UChar32 uc;
	UErrorCode status;

    uiter_setString(&iter, ustring, uchar_len);
	
	new_ustring = (UChar*)lua_newuserdata(L, lua_objlen(L,1));
	
    iter.move(&iter, 0, UITER_LIMIT);
	for (uc = uiter_previous32(&iter); uc != U_SENTINEL; uc = uiter_previous32(&iter)) {
		status = U_ZERO_ERROR;
		u_strFromUTF32(new_ustring, uchar_len, NULL, &uc, 1, &status);
		if (U_FAILURE(status)) {
			lua_pushnil(L);
			lua_pushstring(L, u_errorName(status));
			return 2;
		}
		new_ustring++;
		uchar_len--;
	}
	
	lua_pushlstring(L, (const char*)lua_touserdata(L,-1), lua_objlen(L,-1));
	icu4lua_internrawustring(L, USTRING_UV_META, USTRING_UV_POOL);
	return 1;
}

static int icu_ustring_upper(lua_State *L) {
    UChar* ustring;
    int32_t uchar_len;
    UChar* new_ustring;
	int32_t new_uchar_len;
    UErrorCode status;

	lua_settop(L,2);
	luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,USTRING_UV_META), 1, "expecting ustring");
	lua_pop(L,1);

    ustring = icu4lua_trustustring(L,1);
	uchar_len = (int32_t)icu4lua_ustrlen(L,1);

	// Calculate new length by pre-flighting
	// ICU's manual warns that the result *can* be a different length, but I don't know of any actual test cases
    status = U_ZERO_ERROR;
    new_uchar_len = u_strToUpper(NULL, 0, ustring, uchar_len, lua_isnoneornil(L,2) ? NULL : lua_tostring(L,2), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
	
	// Allocate new ustring
	new_ustring = (UChar*)malloc(sizeof(UChar) * new_uchar_len);
	status = U_ZERO_ERROR;
	u_strToUpper(new_ustring, new_uchar_len, ustring, uchar_len, lua_isnoneornil(L,2) ? NULL : lua_tostring(L,2), &status);
    if (U_FAILURE(status)) {
		free(new_ustring);
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }

	icu4lua_pushustring(L, new_ustring, new_uchar_len, USTRING_UV_META, USTRING_UV_POOL);
	free(new_ustring);
    return 1;
}

static int icu_ustring_lower(lua_State *L) {
    UChar* ustring = icu4lua_checkustring(L,1,USTRING_UV_META);
    int32_t uchar_len = (int32_t)icu4lua_ustrlen(L,1);
    UChar* new_ustring;
	int32_t new_uchar_len;
    UErrorCode status;

	// Calculate new length by pre-flighting
	// ICU's manual warns that the result *can* be a different length, but I don't know of any actual test cases
    status = U_ZERO_ERROR;
    new_uchar_len = u_strToLower(NULL, 0, ustring, uchar_len, lua_isnoneornil(L,2) ? NULL : lua_tostring(L,2), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
	
	// Allocate new ustring
	new_ustring = (UChar*)malloc(sizeof(UChar) * new_uchar_len);
	status = U_ZERO_ERROR;
	u_strToLower(new_ustring, new_uchar_len, ustring, uchar_len, lua_isnoneornil(L,2) ? NULL : lua_tostring(L,2), &status);
    if (U_FAILURE(status)) {
		free(new_ustring);
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }

	icu4lua_pushustring(L, new_ustring, new_uchar_len, USTRING_UV_META, USTRING_UV_POOL);
    return 1;
}

static int icu_ustring_codepoint(lua_State *L) {
    UChar* ustring = icu4lua_checkustring(L,1,USTRING_UV_META);
    int32_t uchar_len = (int32_t)icu4lua_ustrlen(L,2);
    UCharIterator iter;
    int start_pos;
    int end_pos;
    int i;
    UChar32 ch;
    uint32_t end_ucharpos;

    start_pos = luaL_optint(L,2,1);
    end_pos = luaL_optint(L,3,start_pos);

	lua_settop(L,1);
	uiter_setString(&iter, ustring, uchar_len);

    if (end_pos == 0) {
        return 0;
    }
    else if (end_pos < 0) {
        iter.move(&iter, 0, UITER_LIMIT);
        for (i = 0; i > end_pos; i--) {
            if (!iter.hasPrevious(&iter)) {
                return 0;
            }
            iter.move(&iter, -1, UITER_CURRENT);
        }
    }
    else {
        iter.move(&iter, end_pos-1, UITER_START);
    }
    end_ucharpos = iter.getState(&iter);

    if (start_pos < 0) {
        iter.move(&iter, start_pos, UITER_LIMIT);
    }
    else {
        iter.move(&iter, 0, UITER_START);
        for (i = 1; i < start_pos; i++) {
            if (!iter.hasNext(&iter)) {
                return 0;
            }
            iter.move(&iter, 1, UITER_CURRENT);
        }
    }

	while ((iter.getState(&iter)) <= end_ucharpos) {
        ch = iter.next(&iter);
        if (ch == U_SENTINEL) {
            break;
        }
        lua_pushinteger(L,ch);
    }
    return lua_gettop(L)-1;
}

static int icu_ustring_char(lua_State *L) {
	int i = 1;
	int i_max = lua_gettop(L);
	UChar32* utf32 = (UChar32*)malloc(sizeof(UChar32) * i_max);
	UChar* new_ustring;
	UErrorCode status = U_ZERO_ERROR;
	int32_t outLength;
	for (; i <= i_max; i++) {
		utf32[i-1] = (UChar32)lua_tointeger(L,i);
		if (!lua_isnumber(L,i) || utf32[i-1] < 0 || utf32[i-1] >= 0x110000) {
			free(utf32);
			utf32 = NULL;
			luaL_argerror(L,i,"invalid Unicode codepoint");
		}
	}
	// Pre-flighting to find the length of the new ustring
	u_strFromUTF32(NULL, 0, &outLength, utf32, i_max, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		free(utf32);
		utf32 = NULL;
		lua_pushnil(L);
		lua_pushfstring(L, "preflight error:");
		return 2;
	}
	status = U_ZERO_ERROR;
	new_ustring = (UChar*)malloc(sizeof(UChar) * outLength);
	u_strFromUTF32(new_ustring, outLength, NULL, utf32, i_max, &status);
	free(utf32);
	utf32 = NULL;
	if (U_FAILURE(status)) {
		free(new_ustring);
		lua_pushnil(L);
		lua_pushfstring(L, "stringify error: ");
		return 2;
	}
	icu4lua_pushustring(L, new_ustring, outLength, USTRING_UV_META, USTRING_UV_POOL);
	free(new_ustring);
	return 1;
}

static void ustring_pushrange(UMatchState* ms, uint32_t start_state, uint32_t end_state) {
	icu4lua_pushustring(ms->L,
		(const UChar*)(ms->context) + start_state,
		(end_state - start_state),
		USTRING_UV_META, USTRING_UV_POOL);
}

static int icu_ustring_match(lua_State *L) {
	UCharIterator sourceIter;
	UCharIterator pattIter;
	const UChar* string_ustring = icu4lua_checkustring(L,1,USTRING_UV_META);
	const UChar* patt_ustring = icu4lua_checkustring(L,2,USTRING_UV_META);
	UMatchState ms;
	int init;

	uiter_setString(&sourceIter, string_ustring, (int32_t)icu4lua_ustrlen(L,1));
	uiter_setString(&pattIter, patt_ustring, (int32_t)icu4lua_ustrlen(L,2));
	init = luaL_optint(L,3,0);
	ms.L = L;
	ms.pushRange = ustring_pushrange;
	ms.context = (void*)string_ustring;

	return iter_match(&ms, &pattIter, &sourceIter, init, 0);
}

static int icu_ustring_find(lua_State *L) {
	UChar* source_ustring = icu4lua_checkustring(L,1,USTRING_UV_META);
	int32_t source_uchar_len = (int32_t)icu4lua_ustrlen(L,1);
	UChar* patt_ustring = icu4lua_checkustring(L,2,USTRING_UV_META);
	int32_t patt_uchar_len = (int32_t)icu4lua_ustrlen(L,2);
	UCharIterator sourceIter, pattIter;
	UMatchState ms;

	if (lua_toboolean(L,4)) {
		UErrorCode status;
		int init = luaL_optint(L,3,0);
		int offset = 0;
		UChar* found;
		uiter_setString(&sourceIter, source_ustring, source_uchar_len);
		if (init) {
			if (init < 0) {
				sourceIter.move(&sourceIter, init, UITER_LIMIT);
			}
			else {
				sourceIter.move(&sourceIter, init-1, UITER_ZERO);
			}
			offset = uiter_getState(&sourceIter);
		}
		found = u_strFindFirst(source_ustring + offset, source_uchar_len - offset, patt_ustring, patt_uchar_len);
		if (!found) {
			lua_pushnil(L);
			return 1;
		}

		status = U_ZERO_ERROR;
		uiter_setState(&sourceIter, (uint32_t)(found - source_ustring), &status);
		lua_pushinteger(L, 1 + sourceIter.getIndex(&sourceIter, UITER_CURRENT));

		status = U_ZERO_ERROR;
		uiter_setState(&sourceIter, (uint32_t)((found + patt_uchar_len) - source_ustring), &status);
		lua_pushinteger(L, sourceIter.getIndex(&sourceIter, UITER_CURRENT));

		return 2;
	}

	uiter_setString(&sourceIter, source_ustring, source_uchar_len);
	uiter_setString(&pattIter, patt_ustring, patt_uchar_len);
	ms.L = L;
	ms.pushRange = ustring_pushrange;
	ms.context = (void*)source_ustring;

	return iter_match(&ms, &pattIter, &sourceIter, luaL_optint(L,3,0), 1);
}

static void ustring_addrange(UMatchState* ms, uint32_t start_state, uint32_t end_state) {
	icu4lua_addustring(ms->b, (UChar*)ms->context + start_state, end_state - start_state);
}

static void ustring_addmatch(UMatchState* ms) {
	lua_State *L = ms->L;
	luaL_Buffer *b = ms->b;
	switch(lua_type(L,3)) {
		// Userdata replacements will have been checked already for the right metatable
		case LUA_TUSERDATA: {
			const UChar* replace_ustring = icu4lua_trustustring(L,3);
			UCharIterator replaceIter;
			UChar32 c;
			uint32_t start_state;
			int32_t replace_uchar_len = (int32_t)icu4lua_ustrlen(L,3);
			uiter_setString(&replaceIter, replace_ustring, replace_uchar_len);
			start_state = uiter_getState(&replaceIter);
			for (c = uiter_next32(&replaceIter); c != U_SENTINEL; c = uiter_next32(&replaceIter)) {
				if (c == L_ESC) {
					uint32_t new_state;
					uiter_previous32(&replaceIter);
					new_state = uiter_getState(&replaceIter);
					uiter_next32(&replaceIter);
					icu4lua_addustring(b, replace_ustring + start_state, new_state - start_state);
					c = uiter_current32(&replaceIter);
					if (c == '0') {
						uiter_next32(&replaceIter);
						ms->addRange(ms, ms->capture[0].start_state, ms->end_state);
						start_state = uiter_getState(&replaceIter);
					}
					else if (isdigit(c)) {
						int num = c - '1';
						if (num >= ms->level) {
							luaL_error(L, "invalid capture index");
						}
						ms->addRange(ms, ms->capture[num].start_state, ms->capture[num].end_state);
						uiter_next32(&replaceIter);
						start_state = uiter_getState(&replaceIter);
					}
					else {
						start_state = new_state;
					}
				}
			}
			icu4lua_addustring(b, replace_ustring + start_state, replace_uchar_len - start_state);
			return;
		}
		case LUA_TFUNCTION:
			if (ms->level == 0) {
				lua_pushvalue(L, 3);
				ms->pushRange(ms, ms->capture[0].start_state, ms->end_state);
				lua_call(L,1,1);
			}
			else {
				int i;
				lua_pushvalue(L, 3);
				for (i = 0; i < ms->level; i++) {
					ms->pushRange(ms, ms->capture[i].start_state, ms->capture[i].end_state);
				}
				lua_call(L,ms->level,1);
			}
			break;
		case LUA_TTABLE:
			ms->pushRange(ms, ms->capture[0].start_state, ms->end_state);
			lua_gettable(L,3);
			break;
		default:
			luaL_argerror(L, 3, "ustring/function/table expected");
			break;
	}
	if (!lua_toboolean(L,-1)) {
		lua_pop(L,1);
		ms->addRange(ms, ms->capture[0].start_state, ms->end_state);
	}
	else {
		if (!(lua_getmetatable(L,-1) && lua_rawequal(L,-1,USTRING_UV_META))) {
			luaL_error(L, "replacement function/table must either yield a ustring or nil/false");
		}
		lua_pop(L,1);
		luaL_addlstring(b, (const char*)lua_touserdata(L,-1), lua_objlen(L,-1));
		lua_pop(L,1);
	}
}

static int icu_ustring_gsub(lua_State *L) {
	UCharIterator sourceIter;
	UCharIterator pattIter;
	UChar* string_ustring = icu4lua_checkustring(L,1,USTRING_UV_META);
	UChar* patt_ustring = icu4lua_checkustring(L,2,USTRING_UV_META);
	size_t string_uchar_len = icu4lua_ustrlen(L,1);
	size_t patt_uchar_len = icu4lua_ustrlen(L,2);
	UMatchState ms;
	int max_s;
	int replacements;
	luaL_Buffer b;

	if (lua_isuserdata(L,3)) {
		icu4lua_checkustring(L,3,USTRING_UV_META);
	}

	max_s = luaL_optint(L, 4, string_uchar_len+1);

	uiter_setString(&sourceIter, string_ustring, (int32_t)string_uchar_len);
	uiter_setString(&pattIter, patt_ustring, (int32_t)patt_uchar_len);

	ms.L = L;
	ms.context = (void*)string_ustring;
	ms.b = &b;
	ms.pushRange = ustring_pushrange;
	ms.addRange = ustring_addrange;

	luaL_buffinit(L, &b);

	replacements = uiter_gsub_aux(&ms, &pattIter, &sourceIter, ustring_addmatch, max_s);

	icu4lua_pushuresult(&b, USTRING_UV_META, USTRING_UV_POOL);

	lua_pushinteger(L, replacements);
	return 2;
}

static int icu_ustring_gmatch(lua_State *L) {
	UChar* source_ustring = icu4lua_checkustring(L, 1, USTRING_UV_META);
	UChar* patt_ustring = icu4lua_checkustring(L, 2, USTRING_UV_META);
	size_t source_uchar_len = icu4lua_ustrlen(L, 1);
	size_t patt_uchar_len = icu4lua_ustrlen(L, 2);
	GmatchState* gms;
	lua_pushvalue(L, USTRING_UV_META);
	lua_insert(L,1);
	lua_pushvalue(L, USTRING_UV_POOL);
	lua_insert(L,2);
	gms = (GmatchState*)lua_newuserdata(L, sizeof(GmatchState));
	lua_insert(L,3);
	// [meta] [pool] [gms] [source] [patt]
	gms->matched_empty_end = 0;
	gms->ms.L = L;
	gms->ms.pushRange = ustring_pushrange;
	gms->ms.context = (void*)source_ustring;
	uiter_setString(&(gms->sourceIter), source_ustring, (int32_t)source_uchar_len);
	uiter_setString(&(gms->pattIter), patt_ustring, (int32_t)patt_uchar_len);
	gms->source_state = uiter_getState(&(gms->sourceIter));
	lua_pushcclosure(L, gmatch_aux, 5);
	return 1;
}

static int icu_ustring_tconcat(lua_State *L) {
	luaL_Buffer b;
	int i;
	UChar* join;
	int32_t join_len;
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_buffinit(L, &b);
	if (lua_isnoneornil(L,2)) {
		join = NULL;
		join_len = 0;
	}
	else {
		join = icu4lua_checkustring(L,2,USTRING_UV_META);
		join_len = (int32_t)icu4lua_ustrlen(L,2);
	}
	for (i=1;;i++) {
		lua_pushinteger(L,i);
		lua_gettable(L,1);
		if (lua_isnil(L,-1)) {
			break;
		}
		if (join && i > 1) {
			icu4lua_addustring(&b, join, join_len);
		}
		if (!(lua_getmetatable(L,-1) && lua_rawequal(L,-1,USTRING_UV_META) && (lua_pop(L,1),1))) {
			return luaL_argerror(L, 1, "all elements must be ustrings");
		}
		icu4lua_addustring(&b, icu4lua_trustustring(L,-1), icu4lua_ustrlen(L,-1));
	}
	icu4lua_pushuresult(&b, USTRING_UV_META, USTRING_UV_POOL);
	return 1;
}

static int icu_ustring_toraw(lua_State *L) {
	icu4lua_checkustring(L,1,USTRING_UV_META);
	icu4lua_pushrawustring(L,1);
	return 1;
}

static int icu_ustring_fromraw(lua_State *L) {
	luaL_checktype(L,1,LUA_TSTRING);
	luaL_argcheck(L, 0 == (lua_objlen(L,1) % sizeof(UChar)), 1, "invalid length for a raw ustring");
	lua_settop(L,1);
	icu4lua_internrawustring(L, USTRING_UV_META, USTRING_UV_POOL);
	return 1;
}

static int icu_ustring_lib__call(lua_State *L) {
	lua_remove(L,1);
	return icu_ustring_decode(L);
}

static int icu_ustring__concat(lua_State *L) {
	if (!lua_getmetatable(L,1) || !lua_getmetatable(L,2) || !lua_rawequal(L,-2,-1)) {
		return luaL_error(L, "ustrings can only be concatenated to other ustrings");
	}
	icu4lua_pushrawustring(L, 1);
	icu4lua_pushrawustring(L, 2);
	lua_concat(L,2);
	icu4lua_internrawustring(L, USTRING_UV_META, USTRING_UV_POOL);
	return 1;
}

static int icu_ustring__lt(lua_State *L) {
	if (!lua_getmetatable(L,1) || !lua_getmetatable(L,2) || !lua_rawequal(L,-2,-1)) {
		return luaL_error(L, "ustrings can only be compared to other ustrings");
	}
	lua_pushboolean(L, u_strCompare(
		icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1),
		icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
		TRUE
	) < 0);
	return 1;
}

static int icu_ustring__le(lua_State *L) {
	if (!lua_getmetatable(L,1) || !lua_getmetatable(L,2) || !lua_rawequal(L,-2,-1)) {
		return luaL_error(L, "ustrings can only be compared to other ustrings");
	}
	lua_pushboolean(L, u_strCompare(
		icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1),
		icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
		TRUE
	) <= 0);
	return 1;
}

static int icu_ustring_isustring(lua_State *L) {
	lua_pushboolean(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,USTRING_UV_META));
	return 1;
}

static int icu_ustring_lessthan(lua_State *L) {
	icu4lua_checkustring(L,1,USTRING_UV_META);
	icu4lua_checkustring(L,2,USTRING_UV_META);
	if (lua_isboolean(L,3)) {
		UErrorCode status = U_ZERO_ERROR;
		lua_pushboolean(L, u_strCaseCompare(
			icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1),
			icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
			U_FOLD_CASE_DEFAULT,
			&status
		) < 0);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
	}
	else {
		lua_pushboolean(L, u_strCompare(
			icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1),
			icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
			TRUE
		) < 0);
	}
	return 1;
}

static int icu_ustring_lessorequal(lua_State *L) {
	icu4lua_checkustring(L,1,USTRING_UV_META);
	icu4lua_checkustring(L,2,USTRING_UV_META);
	if (lua_isboolean(L,3)) {
		UErrorCode status = U_ZERO_ERROR;
		lua_pushboolean(L, u_strCaseCompare(
			icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1),
			icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
			U_FOLD_CASE_DEFAULT,
			&status
		) <= 0);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
	}
	else {
		lua_pushboolean(L, u_strCompare(
			icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1),
			icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
			TRUE
		) <= 0);
	}
	return 1;
}

static int icu_ustring_equals(lua_State *L) {
	icu4lua_checkustring(L,1,USTRING_UV_META);
	icu4lua_checkustring(L,2,USTRING_UV_META);
	if (lua_toboolean(L,3)) {
		UErrorCode status = U_ZERO_ERROR;
		lua_pushboolean(L, u_strCaseCompare(
			icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1),
			icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
			U_FOLD_CASE_DEFAULT,
			&status
		) == 0);
		if (U_FAILURE(status)) {
			lua_pushstring(L, u_errorName(status));
			return lua_error(L);
		}
	}
	else {
		lua_pushboolean(L, u_strCompare(
			icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1),
			icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
			TRUE
		) == 0);
	}
	return 1;
}

static uint32_t uiter_scanformat(lua_State *L, uint32_t start_state, UCharIterator* pFrmtIter, const UChar* ustrfrmt, UChar *form) {
	uint32_t end_state;
	while (strchr(FLAGS, uiter_current32(pFrmtIter))) {
		uiter_next32(pFrmtIter);
	}
	// TODO: test for repeated flags, show "invalid format (repeated flags)" error
	if (isdigit(uiter_current32(pFrmtIter))) uiter_next32(pFrmtIter); // skip width
	if (isdigit(uiter_current32(pFrmtIter))) uiter_next32(pFrmtIter); // (2 digits at most)
	if (uiter_current32(pFrmtIter) == '.') {
		uiter_next32(pFrmtIter);
		if (isdigit(uiter_current32(pFrmtIter))) uiter_next32(pFrmtIter); // skip precision
		if (isdigit(uiter_current32(pFrmtIter))) uiter_next32(pFrmtIter); // (2 digits at most)
	}
	if (isdigit(uiter_current32(pFrmtIter))) {
		luaL_error(L, "invalid format (width or precision too long)");
	}
	uiter_next32(pFrmtIter);
	end_state = uiter_getState(pFrmtIter);
	u_memcpy(form, ustrfrmt + start_state, end_state - start_state);
	form[end_state - start_state] = '\0';
	return end_state;
}

static void addquoted(lua_State *L, luaL_Buffer *b, int arg) {
	UChar* s = icu4lua_checkustring(L,arg,USTRING_UV_META);
	size_t l = icu4lua_ustrlen(L,arg);
	UChar c = '"';
	icu4lua_addustring(b, &c, 1);
	while (l--) {
		switch (*s) {
			case '"': case '\\': case '\n': {
				c = '\\'; icu4lua_addustring(b, &c, 1);
				icu4lua_addustring(b, s, 1);
				break;
			}
			case '\0': {
				c = '\\'; icu4lua_addustring(b, &c, 1);
				c = '0';
				icu4lua_addustring(b, &c, 1);
				icu4lua_addustring(b, &c, 1);
				icu4lua_addustring(b, &c, 1);
				break;
			}
			default: {
				icu4lua_addustring(b, s, 1);
				break;
			}
		}
		s++;
	}
	c = '"';
	icu4lua_addustring(b, &c, 1);
}

#define U_LUA_INTFRMLEN_LENGTH 1
U_STRING_DECL(U_LUA_INTFRMLEN, "l", 1);

static void addintlen(UChar* form) {
	size_t l = u_strlen(form);
	UChar spec = form[l - 1];
	u_strcpy(form + l - 1, U_LUA_INTFRMLEN);
	form[l + U_LUA_INTFRMLEN_LENGTH - 2] = spec;
	form[l + U_LUA_INTFRMLEN_LENGTH - 1] = '\0';
}

static int icu_ustring_format(lua_State *L) {
	int arg = 1;
	UChar* ustrfrmt = icu4lua_checkustring(L,1,USTRING_UV_META);
	int32_t ustrfrmt_len = (int32_t)icu4lua_ustrlen(L,1);
	UCharIterator frmtIter;
	luaL_Buffer b;
	uint32_t start_state, end_state;
	UChar32 c;
	
	uiter_setString(&frmtIter, ustrfrmt, ustrfrmt_len);

	luaL_buffinit(L, &b);
	start_state = uiter_getState(&frmtIter);
	for (c = uiter_next32(&frmtIter); c != U_SENTINEL; c = uiter_next32(&frmtIter)) {
		if (c == L_ESC) {
			uiter_previous32(&frmtIter);
			end_state = uiter_getState(&frmtIter);
			uiter_next32(&frmtIter);
			icu4lua_addustring(&b, ustrfrmt + start_state, end_state - start_state);

			c = uiter_current32(&frmtIter);
			if (c == L_ESC) {
				start_state = uiter_getState(&frmtIter);
				uiter_next32(&frmtIter);
			}
			else {
				UChar form[MAX_FORMAT];  /* to store the format (`%...') */
				UChar buff[MAX_ITEM];  /* to store the formatted item */
				arg++;
				start_state = uiter_scanformat(L, end_state, &frmtIter, ustrfrmt, form);
				uiter_previous32(&frmtIter);
				switch(uiter_next32(&frmtIter)) {
					case 'c':
						form[1] = 'C';
						u_sprintf_u(buff, form, (int)luaL_checknumber(L, arg));
						break;
					case 'd': case 'i':
						addintlen(form);
						u_sprintf_u(buff, form, (LUA_INTFRM_T)luaL_checknumber(L, arg));
						break;
					case 'o': case 'u': case 'x': case 'X':
						addintlen(form);
						u_sprintf_u(buff, form, (unsigned LUA_INTFRM_T)luaL_checknumber(L, arg));
						break;
					case 'e': case 'E': case 'f': case 'g': case 'G':
						u_sprintf_u(buff, form, (double)luaL_checknumber(L, arg));
						break;
					case 'q':
						addquoted(L, &b, arg);
						continue;
					case 's': {
						luaL_argcheck(L, lua_getmetatable(L,arg) && lua_rawequal(L,-1,USTRING_UV_META), arg, "expecting ustring");
						luaL_addlstring(&b, (const char*)lua_touserdata(L,arg), lua_objlen(L,arg));
						continue;
					}
					default:
						return luaL_error(L, "invalid option to " LUA_QL("format"));
				}
				icu4lua_addustring(&b, buff, u_strlen(buff));
			}
		}
	}
	icu4lua_addustring(&b, ustrfrmt + start_state, ustrfrmt_len - start_state);
	icu4lua_pushuresult(&b, USTRING_UV_META, USTRING_UV_POOL);
	return 1;
}

static int icu_ustring_poolsize(lua_State* L) {
	int ustrings = 0;
	int32_t uchars = 0;
	lua_gc(L, LUA_GCSTOP, 0);
	lua_pushnil(L);
	while (lua_next(L,USTRING_UV_POOL)) {
		ustrings++;
		uchars += (int32_t)icu4lua_ustrlen(L,-1);
		lua_pop(L,1);
	}
	lua_gc(L, LUA_GCRESTART, 0);
	lua_pushinteger(L, ustrings);
	lua_pushinteger(L, uchars);
	return 2;
}

const static luaL_Reg icu_ustring_lib[] = {
	{"decode", icu_ustring_decode},
	{"encode", icu_ustring_encode},
	{"unescape", icu_ustring_unescape},

	{"isustring", icu_ustring_isustring},

	{"lessthan", icu_ustring_lessthan},
	{"lessorequal", icu_ustring_lessorequal},
	{"equals", icu_ustring_equals},

	{"len", icu_ustring_len},
	{"rep", icu_ustring_rep},
	{"sub", icu_ustring_sub},
	{"reverse", icu_ustring_reverse},
	{"upper", icu_ustring_upper},
	{"lower", icu_ustring_lower},
	{"codepoint", icu_ustring_codepoint},
	{"char", icu_ustring_char},
	{"format", icu_ustring_format},
	{"match", icu_ustring_match},
	{"gsub", icu_ustring_gsub},
	{"find", icu_ustring_find},
	{"gmatch", icu_ustring_gmatch},

	{"tconcat", icu_ustring_tconcat},
	{"toraw", icu_ustring_toraw},
	{"fromraw", icu_ustring_fromraw},

	{"poolsize", icu_ustring_poolsize},

	{NULL, NULL}
};

int luaopen_icu_ustring(lua_State *L) {
	int IDX_USTRING_META, IDX_USTRING_POOL, IDX_USTRING_LIB;
	const luaL_Reg* lib_entry;
	luaL_Reg null_entry = {NULL,NULL};

	// Create the ustring metatable
	luaL_newmetatable(L, "icu.ustring");
	IDX_USTRING_META = lua_gettop(L);

	// Create the ustring pool
	lua_newtable(L);
	IDX_USTRING_POOL = lua_gettop(L);

	// Put the ustring pool in the registry (to keep it from being garbage)
	lua_pushvalue(L, IDX_USTRING_POOL);
	lua_setfield(L, LUA_REGISTRYINDEX, "icu.ustring pool");

	// Give the ustring pool weak values
	lua_newtable(L);
	lua_pushliteral(L, "v");
	lua_setfield(L,-2,"__mode");
	lua_setmetatable(L,IDX_USTRING_POOL);
	
	// Create the lib table
	luaL_register(L, "icu.ustring", &null_entry);
	IDX_USTRING_LIB = lua_gettop(L);

	// Populate the lib table, adding in the upvalues for the metatable and pool
	for (lib_entry = &icu_ustring_lib[0]; lib_entry->func; lib_entry++) {
		lua_pushstring(L, lib_entry->name);
		lua_pushvalue(L, IDX_USTRING_META);
		lua_pushvalue(L, IDX_USTRING_POOL);
		lua_pushcclosure(L, lib_entry->func, 2);
		lua_rawset(L, IDX_USTRING_LIB);
	}

	// Set the "index" metamethod to lookup the lib table
	lua_pushvalue(L, IDX_USTRING_LIB);
	lua_setfield(L, IDX_USTRING_META, "__index");

	// Set the "tostring" metamethod
	lua_getfield(L, IDX_USTRING_LIB, "encode");
	lua_setfield(L, IDX_USTRING_META, "__tostring");

	// Set the "len" metamethod
	lua_getfield(L, IDX_USTRING_LIB, "len");
	lua_setfield(L, IDX_USTRING_META, "__len");

	// Set the "concat" metamethod
	lua_pushvalue(L, IDX_USTRING_META);
	lua_pushvalue(L, IDX_USTRING_POOL);
	lua_pushcclosure(L, icu_ustring__concat, 2);
	lua_setfield(L, IDX_USTRING_META, "__concat");

	// Set the "lt" metamethod
	lua_pushvalue(L, IDX_USTRING_META);
	lua_pushvalue(L, IDX_USTRING_POOL);
	lua_pushcclosure(L, icu_ustring__lt, 2);
	lua_setfield(L, IDX_USTRING_META, "__lt");

	// Set the "le" metamethod
	lua_pushvalue(L, IDX_USTRING_META);
	lua_pushvalue(L, IDX_USTRING_POOL);
	lua_pushcclosure(L, icu_ustring__le, 2);
	lua_setfield(L, IDX_USTRING_META, "__le");

	// Allow icu.ustring(...) to be equivalent to icu.ustring.decode(...)
	lua_pushvalue(L, IDX_USTRING_META);
	lua_pushvalue(L, IDX_USTRING_POOL);
	lua_pushcclosure(L, icu_ustring_lib__call, 2);
	lua_setfield(L, IDX_USTRING_LIB, "__call");
	lua_pushvalue(L, IDX_USTRING_LIB);
	lua_setmetatable(L, IDX_USTRING_LIB);

	// Setting icu.ustring.empty
	lua_newuserdata(L, 0);
	lua_pushvalue(L, IDX_USTRING_META);
	lua_setmetatable(L,-2);
	lua_pushliteral(L, "");
	lua_pushvalue(L,-2);
	lua_rawset(L, IDX_USTRING_POOL);
	lua_setfield(L, IDX_USTRING_LIB, "empty");

	// Return the lib table
	lua_settop(L, IDX_USTRING_LIB);
	return 1;
}
