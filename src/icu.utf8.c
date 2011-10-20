
// icu.utf8 is based heavily on Lua 5.1's lstrlib.c as it is intended to provide equivalent functionality.
// Some functions are identical, like icu.utf8.rep, while others are near identical, like icu.utf8.format
// (only %c is different).

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>
#include "matchengine.h"
#include "formatting.h"

static int icu_utf8_unescape(lua_State *L) {
	UChar* temp_ustring;
	int32_t uchar_len;
	char* new_utf8;
	int32_t byte_len;
	UErrorCode status;

	uchar_len = u_unescape(luaL_checkstring(L,1), NULL, 0);
	temp_ustring = (UChar*)malloc(sizeof(UChar) * uchar_len);
	u_unescape(lua_tostring(L,1), temp_ustring, uchar_len);
	
	status = U_ZERO_ERROR;
	u_strToUTF8(NULL, 0, &byte_len, temp_ustring, uchar_len, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		free(temp_ustring);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}

	new_utf8 = (char*)malloc(byte_len);
	status = U_ZERO_ERROR;
	u_strToUTF8(new_utf8, byte_len, NULL, temp_ustring, uchar_len, &status);
	if (U_FAILURE(status)) {
		free(temp_ustring);
		free(new_utf8);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}

	lua_pushlstring(L, new_utf8, byte_len);

	free(temp_ustring);
	free(new_utf8);

	return 1;
}

static int count_utf8_codepoints(const char* utf8_string, size_t bytes_left) {
	int count = 0;
	while (bytes_left-- > 0) {
		if ((*utf8_string++ & 0xc0) != 0x80) {
			count++;
		}
	}
	return count;
}

static int icu_utf8_len(lua_State *L) {
	size_t byte_length;
	const char* utf8_string = luaL_checklstring(L,1,&byte_length);
	lua_pushinteger(L, count_utf8_codepoints(utf8_string, byte_length));
	return 1;
}

// same as string.rep!
static int icu_utf8_rep(lua_State *L) {
    size_t l;
    luaL_Buffer b;
    const char *s = luaL_checklstring(L, 1, &l);
    int n = luaL_checkint(L, 2);
    luaL_buffinit(L, &b);
    while (n-- > 0) {
        luaL_addlstring(&b, s, l);
    }
    luaL_pushresult(&b);
    return 1;
}

static int icu_utf8_reverse(lua_State *L) {
    size_t length;
    const char* start = luaL_checklstring(L, 1, &length);
    const char* pos = start + length;
    luaL_Buffer b;
    int n;
    luaL_buffinit(L,&b);
    while (--pos >= start) {
        n = 1;
        while ((*pos&0xC0)==0x80) {
            pos--;
            n++;
        }
        luaL_addlstring(&b,pos,n);
    }
    luaL_pushresult(&b);
    return 1;
}

static int icu_utf8_upper(lua_State *L) {
	int32_t byte_len;
    size_t byte_size;
    const char* utf8;
    char* utf8_upper;
    int32_t uchar_len;
    UChar* ustring;
    UErrorCode status;
    int32_t buf_size;
    utf8 = luaL_checklstring(L,1,&byte_size);
	byte_len = (int32_t)byte_size;
    status = U_ZERO_ERROR;
    u_strFromUTF8(NULL, 0, &uchar_len, utf8, (int32_t)byte_len, &status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    status = U_ZERO_ERROR;
    buf_size = sizeof(UChar) * uchar_len * 2; // i assume the worst case is every UChar needs to be 2?
    ustring = (UChar*)malloc(buf_size);
    u_strFromUTF8(ustring, uchar_len, NULL, utf8, (int32_t)byte_len, &status);
    if (U_FAILURE(status)) {
        free(ustring);
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    status = U_ZERO_ERROR;
    u_strToUpper(ustring, buf_size, ustring, uchar_len, lua_isnoneornil(L,2) ? NULL : lua_tostring(L,2), &status);
    if (U_FAILURE(status)) {
        free(ustring);
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    status = U_ZERO_ERROR;
    u_strToUTF8(NULL, 0, &byte_len, ustring, uchar_len, &status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    utf8_upper = (char*)malloc(byte_len);
    status = U_ZERO_ERROR;
    u_strToUTF8(utf8_upper, byte_len, NULL, ustring, uchar_len, &status);
    free(ustring);
    if (U_FAILURE(status)) {
        free(utf8_upper);
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    lua_pushlstring(L, utf8_upper, byte_len);
    free(utf8_upper);
    return 1;
}

static int icu_utf8_lower(lua_State *L) {
    size_t byte_size;
	int32_t byte_len;
    const char* utf8;
    char* utf8_lower;
    int32_t uchar_len;
    UChar* ustring;
    UErrorCode status;
    int32_t buf_size;
    utf8 = luaL_checklstring(L,1,&byte_size);
	byte_len = (int32_t)byte_size;
    status = U_ZERO_ERROR;
    u_strFromUTF8(NULL, 0, &uchar_len, utf8, (int32_t)byte_len, &status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    status = U_ZERO_ERROR;
    buf_size = sizeof(UChar) * uchar_len * 2;
    ustring = (UChar*)malloc(buf_size); // i assume the worst case is every UChar needs to be 2
    u_strFromUTF8(ustring, uchar_len, NULL, utf8, (int32_t)byte_len, &status);
    if (U_FAILURE(status)) {
        free(ustring);
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    status = U_ZERO_ERROR;
    u_strToLower(ustring, buf_size, ustring, uchar_len, lua_isnoneornil(L,2) ? NULL : lua_tostring(L,2), &status);
    if (U_FAILURE(status)) {
        free(ustring);
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    status = U_ZERO_ERROR;
    u_strToUTF8(NULL, 0, &byte_len, ustring, uchar_len, &status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    utf8_lower = (char*)malloc(byte_len);
    status = U_ZERO_ERROR;
    u_strToUTF8(utf8_lower, byte_len, NULL, ustring, uchar_len, &status);
    free(ustring);
    if (U_FAILURE(status)) {
        free(utf8_lower);
        lua_pushnil(L);
        lua_pushstring(L, u_errorName(status));
        return 2;
    }
    lua_pushlstring(L, utf8_lower, byte_len);
    free(utf8_lower);
    return 1;
}

static int icu_utf8_codepoint(lua_State *L) {
    const char* utf8;
    size_t byte_len;
    UCharIterator iter;
    int start_pos;
    int end_pos;
    int i;
    UChar32 ch;
    uint32_t end_bytepos;

    utf8 = luaL_checklstring(L,1,&byte_len);
    start_pos = luaL_optint(L,2,1);
    end_pos = luaL_optint(L,3,start_pos);

    lua_settop(L,1);
    uiter_setUTF8(&iter, utf8, (int32_t)byte_len);

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
    end_bytepos = iter.getState(&iter) >> 1;

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

    while ((iter.getState(&iter) >> 1) <= end_bytepos) {
        ch = iter.next(&iter);
        if (ch == U_SENTINEL) {
            break;
        }
        luaL_checkstack(L,1,"error growing the stack");
        lua_pushinteger(L,ch);
    }
    return lua_gettop(L)-1;
}

static int icu_utf8_sub(lua_State *L) {
    const char* utf8;
    size_t byte_len;
    UCharIterator iter;
    int start_pos;
    int end_pos;
    int i;
    uint32_t start_bytepos;
    uint32_t end_bytepos;

    utf8 = luaL_checklstring(L,1,&byte_len);
    start_pos = luaL_optint(L,2,1);
    end_pos = luaL_optint(L,3,-1);

    lua_settop(L,1);
    uiter_setUTF8(&iter, utf8, (int32_t)byte_len);

    if (end_pos == 0) {
        lua_pushliteral(L, "");
        return 1;
    }
    else if (end_pos < 0) {
        iter.move(&iter, 0, UITER_LIMIT);
        for (i = -1; i > end_pos; i--) {
            if (!iter.hasPrevious(&iter)) {
                lua_pushliteral(L, "");
                return 1;
            }
            iter.move(&iter, -1, UITER_CURRENT);
        }
    }
    else {
        iter.move(&iter, end_pos, UITER_START);
    }
    end_bytepos = iter.getState(&iter) >> 1;

    if (start_pos < 0) {
        iter.move(&iter, start_pos, UITER_LIMIT);
    }
    else {
        iter.move(&iter, 0, UITER_START);
        for (i = 1; i < start_pos; i++) {
            if (!iter.hasNext(&iter)) {
				lua_pushliteral(L,"");
                return 1;
            }
            iter.move(&iter, 1, UITER_CURRENT);
        }
    }
    start_bytepos = iter.getState(&iter) >> 1;

	if (start_bytepos > end_bytepos) {
		lua_pushliteral(L,"");
	}
	else {
		lua_pushlstring(L, utf8 + start_bytepos, (end_bytepos - start_bytepos));
	}

    return 1;
}

static int icu_utf8_char(lua_State *L) {
    int count = lua_gettop(L);
    luaL_Buffer buf;
    int i;
    luaL_buffinit(L,&buf);
    for (i = 0; i < count; i++) {
        int codePoint = luaL_checkint(L,i+1);
        if (codePoint < 0) {
            return luaL_argerror(L,i+1,"expecting positive number");
        }
        else if (codePoint < 0x80) {
            // 00000000 00000000 00000000 0xxxxxxx
            //                            0xxxxxxx
            luaL_addchar(&buf, (char)codePoint);
        }
        else if (codePoint < 0x800) {
            // 00000000 00000000 00000yyy xxxxxxxx
            //                   110yyyxx 10xxxxxx
            luaL_addchar(&buf, (char)(0xC0 | (codePoint >> 6)));
            luaL_addchar(&buf, (char)(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint < 0x10000) {
            // 00000000 00000000 yyyyyyyy xxxxxxxx
            //          1110yyyy 10yyyyxx 10xxxxxx
            luaL_addchar(&buf, (char)(0xE0 | (codePoint >> 12)));
            luaL_addchar(&buf, (char)(0x80 | ((codePoint >> 6) & 0x3F)));
            luaL_addchar(&buf, (char)(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint > 0x110000) {
            return luaL_argerror(L,i+1,"invalid codepoint");
        }
        else {
            // 00000000 000zzzzz yyyyyyyy xxxxxxxx
            // 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx
            luaL_addchar(&buf, (char)(0xF0 | (codePoint >> 18)));
            luaL_addchar(&buf, (char)(0x80 | ((codePoint >> 12) & 0x3F)));
            luaL_addchar(&buf, (char)(0x80 | ((codePoint >> 6) & 0x3F)));
            luaL_addchar(&buf, (char)(0x80 | (codePoint & 0x3F)));
        }
    }
    luaL_pushresult(&buf);
    return 1;
}

static void utf8_pushrange(UMatchState* ms, uint32_t start_state, uint32_t end_state) {
	lua_pushlstring(ms->L, (const char*)(ms->context) + (start_state >> 1), (end_state >> 1) - (start_state >> 1));
}

static int icu_utf8_match(lua_State *L) {
	UCharIterator sourceIter;
	UCharIterator pattIter;
	const char* string_utf8;
	size_t string_utf8_len;
	const char* patt_utf8;
	size_t patt_utf8_len;
	UMatchState ms;
	int init;

	string_utf8 = luaL_checklstring(L,1,&string_utf8_len);
	patt_utf8 = luaL_checklstring(L,2,&patt_utf8_len);
	uiter_setUTF8(&sourceIter, string_utf8, (int32_t)string_utf8_len);
	uiter_setUTF8(&pattIter, patt_utf8, (int32_t)patt_utf8_len);
	init = luaL_optint(L,3,0);
	ms.L = L;
	ms.pushRange = utf8_pushrange;
	ms.context = (void*)string_utf8;

	return iter_match(&ms, &pattIter, &sourceIter, init, 0);
}

static const char *lmemfind (const char *s1, size_t l1, const char *s2, size_t l2) {
	if (l2 == 0) {
		return s1;  /* empty strings are everywhere */
	}
	else if (l2 > l1) {
		return NULL;  /* avoids a negative `l1' */
	}
	else {
		const char *init;  /* to search for a `*s2' inside `s1' */
		l2--;  /* 1st char will be checked by `memchr' */
		l1 = l1-l2;  /* `s2' cannot be found after that */
		while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
			init++;   /* 1st char is already checked */
			if (memcmp(init, s2+1, l2) == 0) {
				return init-1;
			}
			else {  /* correct `l1' and `s1' to try again */
				l1 -= init-s1;
				s1 = init;
			}
		}
		return NULL;  /* not found */
	}
}

static int icu_utf8_find(lua_State *L) {
	size_t source_byte_len, patt_byte_len;
	const char* source_utf8 = luaL_checklstring(L, 1, &source_byte_len);
	const char* patt_utf8 = luaL_checklstring(L, 2, &patt_byte_len);
	UCharIterator sourceIter, pattIter;
	UMatchState ms;

	uiter_setUTF8(&sourceIter, source_utf8, (int32_t)source_byte_len);
	uiter_setUTF8(&pattIter, patt_utf8, (int32_t)patt_byte_len);

	ms.L = L;
	ms.pushRange = utf8_pushrange;
	ms.context = (void*)source_utf8;

	return iter_match(&ms, &pattIter, &sourceIter, luaL_optint(L,3,0), 1);
}

static void utf8_addrange(UMatchState* ms, uint32_t start_state, uint32_t end_state) {
	luaL_addlstring(ms->b, (const char*)(ms->context) + (start_state >> 1), (end_state >> 1) - (start_state >> 1));
}

static void utf8_addmatch(UMatchState* ms) {
	lua_State *L = ms->L;
	luaL_Buffer *b = ms->b;
	switch(lua_type(L,3)) {
		case LUA_TSTRING:
		case LUA_TNUMBER: {
			size_t l, i;
			const char *news = lua_tolstring(L, 3, &l);
			for (i = 0; i < l; i++) {
				if (news[i] != L_ESC) {
					luaL_addchar(b, news[i]);
				}
				else {
					i++;  // skip ESC
					if (!isdigit(uchar(news[i]))) {
						luaL_addchar(b, news[i]);
					}
					else if (news[i] == '0') {
						ms->addRange(ms, ms->capture[0].start_state, ms->end_state);
					}
					else {
						int cnum = news[i] - '1';
						if (cnum >= ms->level) {
							luaL_error(L, "invalid capture index");
						}
						ms->addRange(ms, ms->capture[cnum].start_state, ms->capture[cnum].end_state);
					}
				}
			}
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
			luaL_argerror(L, 3, "string/function/table expected");
			break;
	}
	if (!lua_toboolean(L,-1)) {
		lua_pop(L,1);
		ms->pushRange(ms, ms->capture[0].start_state, ms->end_state);
	}
	luaL_addvalue(b);
}

static int icu_utf8_gsub(lua_State *L) {
	UCharIterator sourceIter;
	UCharIterator pattIter;
	size_t string_byte_len, patt_byte_len;
	const char* string_utf8 = luaL_checklstring(L, 1, &string_byte_len);
	const char* patt_utf8 = luaL_checklstring(L, 2, &patt_byte_len);
	UMatchState ms;
	int max_s = luaL_optint(L, 4, string_byte_len+1);
	int replacements;
	luaL_Buffer b;

	uiter_setUTF8(&sourceIter, string_utf8, (int32_t)string_byte_len);
	uiter_setUTF8(&pattIter, patt_utf8, (int32_t)patt_byte_len);

	ms.L = L;
	ms.context = (void*)string_utf8;
	ms.b = &b;
	ms.pushRange = utf8_pushrange;
	ms.addRange = utf8_addrange;

	luaL_buffinit(L, &b);

	replacements = uiter_gsub_aux(&ms, &pattIter, &sourceIter, utf8_addmatch, max_s);

	luaL_pushresult(&b);
	lua_pushinteger(L, replacements);
	return 2;
}

static int icu_utf8_gmatch(lua_State *L) {
	size_t source_byte_len, patt_byte_len;
	const char* source_utf8 = luaL_checklstring(L, 1, &source_byte_len);
	const char* patt_utf8 = luaL_checklstring(L, 2, &patt_byte_len);
	GmatchState* gms;
	lua_settop(L, 2);
	gms = (GmatchState*)lua_newuserdata(L, sizeof(GmatchState));
	gms->matched_empty_end = 0;
	gms->ms.L = L;
	gms->ms.pushRange = utf8_pushrange;
	gms->ms.context = (void*)source_utf8;
	uiter_setUTF8(&(gms->sourceIter), source_utf8, (int32_t)source_byte_len);
	uiter_setUTF8(&(gms->pattIter), patt_utf8, (int32_t)patt_byte_len);
	gms->source_state = uiter_getState(&(gms->sourceIter));
	lua_pushcclosure(L, gmatch_aux, 3); // strings kept in upvalues just to keep them from being garbage
	return 1;
}

static void addquoted (lua_State *L, luaL_Buffer *b, int arg) {
  size_t l;
  const char *s = luaL_checklstring(L, arg, &l);
  luaL_addchar(b, '"');
  while (l--) {
    switch (*s) {
      case '"': case '\\': case '\n': {
        luaL_addchar(b, '\\');
        luaL_addchar(b, *s);
        break;
      }
      case '\0': {
        luaL_addlstring(b, "\\000", 4);
        break;
      }
      default: {
        luaL_addchar(b, *s);
        break;
      }
    }
    s++;
  }
  luaL_addchar(b, '"');
}

static const char *scanformat (lua_State *L, const char *strfrmt, char *form) {
	const char *p = strfrmt;
	while (strchr(FLAGS, *p)) p++;  /* skip flags */
	if ((size_t)(p - strfrmt) >= sizeof(FLAGS)) {
		luaL_error(L, "invalid format (repeated flags)");
	}
	if (isdigit(uchar(*p))) p++;  /* skip width */
	if (isdigit(uchar(*p))) p++;  /* (2 digits at most) */
	if (*p == '.') {
		p++;
		if (isdigit(uchar(*p))) p++;  /* skip precision */
		if (isdigit(uchar(*p))) p++;  /* (2 digits at most) */
	}
	if (isdigit(uchar(*p))) {
		luaL_error(L, "invalid format (width or precision too long)");
	}
	*(form++) = '%';
	strncpy(form, strfrmt, p - strfrmt + 1);
	form += p - strfrmt + 1;
	*form = '\0';
	return p;
}


static void addintlen (char *form) {
	size_t l = strlen(form);
	char spec = form[l - 1];
	strcpy(form + l - 1, LUA_INTFRMLEN);
	form[l + sizeof(LUA_INTFRMLEN) - 2] = spec;
	form[l + sizeof(LUA_INTFRMLEN) - 1] = '\0';
}

static int icu_utf8_format (lua_State *L) {
	int arg = 1;
	size_t sfl;
	const char *strfrmt = luaL_checklstring(L, arg, &sfl);
	const char *strfrmt_end = strfrmt+sfl;
	luaL_Buffer b;
	luaL_buffinit(L, &b);
	while (strfrmt < strfrmt_end) {
		if (*strfrmt != L_ESC) {
			luaL_addchar(&b, *strfrmt++);
		}
		else if (*++strfrmt == L_ESC) {
			luaL_addchar(&b, *strfrmt++);  /* %% */
		}
		else { /* format item */
			char form[MAX_FORMAT];  /* to store the format (`%...') */
			char buff[MAX_ITEM];  /* to store the formatted item */
			arg++;
			strfrmt = scanformat(L, strfrmt, form);
			switch (*strfrmt++) {
				case 'c': {
					int codePoint = (int)luaL_checknumber(L, arg);
					if (codePoint < 0) {
						return luaL_argerror(L, arg, "invalid codepoint");
					}
					else if (codePoint < 0x80) {
						// 00000000 00000000 00000000 0xxxxxxx
						//                            0xxxxxxx
						luaL_addchar(&b, (char)codePoint);
					}
					else if (codePoint < 0x800) {
						// 00000000 00000000 00000yyy xxxxxxxx
						//                   110yyyxx 10xxxxxx
						luaL_addchar(&b, (char)(0xC0 | (codePoint >> 6)));
						luaL_addchar(&b, (char)(0x80 | (codePoint & 0x3F)));
					}
					else if (codePoint < 0x10000) {
						// 00000000 00000000 yyyyyyyy xxxxxxxx
						//          1110yyyy 10yyyyxx 10xxxxxx
						luaL_addchar(&b, (char)(0xE0 | (codePoint >> 12)));
						luaL_addchar(&b, (char)(0x80 | ((codePoint >> 6) & 0x3F)));
						luaL_addchar(&b, (char)(0x80 | (codePoint & 0x3F)));
					}
					else if (codePoint > 0x110000) {
						return luaL_argerror(L,arg,"invalid codepoint");
					}
					else {
						// 00000000 000zzzzz yyyyyyyy xxxxxxxx
						// 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx
						luaL_addchar(&b, (char)(0xF0 | (codePoint >> 18)));
						luaL_addchar(&b, (char)(0x80 | ((codePoint >> 12) & 0x3F)));
						luaL_addchar(&b, (char)(0x80 | ((codePoint >> 6) & 0x3F)));
						luaL_addchar(&b, (char)(0x80 | (codePoint & 0x3F)));
					}
					continue;
				}
				case 'd':  case 'i': {
					addintlen(form);
					sprintf(buff, form, (LUA_INTFRM_T)luaL_checknumber(L, arg));
					break;
				}
				case 'o':  case 'u':  case 'x':  case 'X': {
					addintlen(form);
					sprintf(buff, form, (unsigned LUA_INTFRM_T)luaL_checknumber(L, arg));
					break;
				}
				case 'e':  case 'E': case 'f':
				case 'g': case 'G': {
					sprintf(buff, form, (double)luaL_checknumber(L, arg));
					break;
				}
				case 'q': {
					addquoted(L, &b, arg);
					continue;  /* skip the 'addsize' at the end */
				}
				case 's': {
					size_t l;
					const char *s = luaL_checklstring(L, arg, &l);
					if (!strchr(form, '.') && l >= 100) {
						/* no precision and string is too long to be formatted;
						   keep original string */
						lua_pushvalue(L, arg);
						luaL_addvalue(&b);
						continue;  /* skip the `addsize' at the end */
					}
					else {
						sprintf(buff, form, s);
						break;
					}
				}
				default: {  /* also treat cases `pnLlh' */
				  return luaL_error(L, "invalid option to " LUA_QL("format"));
				}
			}
			luaL_addlstring(&b, buff, strlen(buff));
		}
	}
	luaL_pushresult(&b);
	return 1;
}


static int icu_utf8_lessthan(lua_State *L) {
	size_t a_len;
	size_t b_len;
	const char* utf8_a = luaL_checklstring(L,1,&a_len);
	const char* utf8_b = luaL_checklstring(L,2,&b_len);
	// Future - take extra parameters for collation, case-insensitive comparison
	{
		UCharIterator iter_a;
		UCharIterator iter_b;
		uiter_setUTF8(&iter_a, utf8_a, (int32_t)a_len);
		uiter_setUTF8(&iter_b, utf8_b, (int32_t)b_len);
		lua_pushboolean(L, u_strCompareIter(&iter_a, &iter_b, TRUE) < 0);
	}
	return 1;
}

static int icu_utf8_lessorequal(lua_State *L) {
	size_t a_len;
	size_t b_len;
	const char* utf8_a = luaL_checklstring(L,1,&a_len);
	const char* utf8_b = luaL_checklstring(L,2,&b_len);
	// Future - take extra parameters for collation, case-insensitive comparison
	{
		UCharIterator iter_a;
		UCharIterator iter_b;
		uiter_setUTF8(&iter_a, utf8_a, (int32_t)a_len);
		uiter_setUTF8(&iter_b, utf8_b, (int32_t)b_len);
		lua_pushboolean(L, u_strCompareIter(&iter_a, &iter_b, TRUE) <= 0);
	}
	return 1;
}

static int icu_utf8_equals(lua_State *L) {
	const char* utf8_a;
	const char* utf8_b;
	size_t a_len;
	size_t b_len;
	utf8_a = luaL_checklstring(L,1,&a_len);
	utf8_b = luaL_checklstring(L,2,&b_len);
	// Future - take extra parameters for collation, case-insensitive comparison
	{
		UCharIterator iter_a;
		UCharIterator iter_b;
		uiter_setUTF8(&iter_a, utf8_a, (int32_t)a_len);
		uiter_setUTF8(&iter_b, utf8_b, (int32_t)b_len);
		lua_pushboolean(L, u_strCompareIter(&iter_a, &iter_b, TRUE) == 0);
	}
	return 1;
}

static int icu_utf8_loadstring(lua_State *L) {
	size_t l;
	const char* s = luaL_checklstring(L, 1, &l);
	const char* chunkname = luaL_optstring(L, 2, s);
	int status;
	if (s[0] == '\xEF' && s[1] == '\xBB' && s[2] == '\xBF') {
		status = luaL_loadbuffer(L, s + 3, l - 3, NULL);
	}
	else {
		status = luaL_loadbuffer(L, s, l, NULL);
	}
	if (status == 0) {
		return 1;
	}
	else {
		lua_pushnil(L);
		lua_insert(L, -2);
		return 2;
	}
}

typedef struct LoadF {
	int extraline;
	FILE *f;
	char buff[LUAL_BUFFERSIZE];
} LoadF;

static const char *getF (lua_State *L, void *ud, size_t *size) {
	LoadF* lf = (LoadF*)ud;
	(void)L;
	if (lf->extraline) {
		lf->extraline = 0;
		*size = 1;
		return "\n";
	}
	if (feof(lf->f)) {
		return NULL;
	}
	*size = fread(lf->buff, 1, LUAL_BUFFERSIZE, lf->f);
	return (*size > 0) ? lf->buff : NULL;
}

static int icu4lua_loadfile(lua_State *L, const char* filename) {
	LoadF lf;
	int c;
	int status, readstatus;
	lf.extraline = 0;
	lf.f = fopen(filename, "r");
	if (lf.f == NULL) {
		lua_pushfstring(L, "could not open %s", filename);
		return 1;
	}
	c = fgetc(lf.f);
	if (c == 0xEF) {
		// Assume this is the first byte of the UTF-8 BOM - if it isn't, it's not going to be valid anyway.
		c = fgetc(lf.f);
		c = fgetc(lf.f);
		c = fgetc(lf.f);
	}
	if (c == '#') {  // Unix exec. file? 
		lf.extraline = 1;
		while ((c = getc(lf.f)) != EOF && c != '\n') ;  // skip first line
		if (c == '\n') c = getc(lf.f);
	}
	if (c == LUA_SIGNATURE[0] && lf.f != stdin) {  // binary file?
		fclose(lf.f);
		lf.f = fopen(filename, "rb");  // reopen in binary mode
		if (lf.f == NULL) {
			lua_pushfstring(L, "unable to reopen %s", filename);
			return 1;
		}
		// skip eventual `#!...'
		while ((c = getc(lf.f)) != EOF && c != LUA_SIGNATURE[0]) ;
		lf.extraline = 0;
	}
	ungetc(c, lf.f);
	status = lua_load(L, getF, &lf, filename);
	readstatus = ferror(lf.f);
	fclose(lf.f);  // close file (even in case of errors)
	if (readstatus) {
		lua_pushfstring(L, "unable to read from %s", filename);
		return 1;
	}
	return 0;
}

static int icu_utf8_loadfile(lua_State *L) {
	if (icu4lua_loadfile(L, luaL_checkstring(L,1)) == 0) {
		return 1;
	}
	else {
		lua_pushnil(L);
		lua_insert(L,-2);
		return 2;
	}
}

static int icu_utf8_dofile(lua_State *L) {
	lua_settop(L,1);
	if (icu4lua_loadfile(L, luaL_checkstring(L,1)) != 0) {
		return lua_error(L);
	}
	lua_call(L,0,LUA_MULTRET);
	return lua_gettop(L) - 1;
}

static const luaL_Reg icu_utf8_lib[] = {
	{"unescape", icu_utf8_unescape},
	{"lessthan", icu_utf8_lessthan},
	{"lessorequal", icu_utf8_lessorequal},
	{"equals", icu_utf8_equals},

	{"len", icu_utf8_len},
	{"rep", icu_utf8_rep},
	{"sub", icu_utf8_sub},
	{"reverse", icu_utf8_reverse},
	{"upper", icu_utf8_upper},
	{"lower", icu_utf8_lower},
	{"codepoint", icu_utf8_codepoint},
	{"char", icu_utf8_char},
	{"match", icu_utf8_match},
	{"gsub", icu_utf8_gsub},
	{"find", icu_utf8_find},
	{"gmatch", icu_utf8_gmatch},
	{"format", icu_utf8_format},

	{"loadstring", icu_utf8_loadstring},
	{"loadfile", icu_utf8_loadfile},
	{"dofile", icu_utf8_dofile},

	{NULL, NULL}
};

int luaopen_icu_utf8(lua_State *L) {
	luaL_register(L, "icu.utf8", icu_utf8_lib);

    lua_pushliteral(L,"\xEF\xBB\xBF");
	lua_setfield(L,-2,"bom");

	return 1;
}
