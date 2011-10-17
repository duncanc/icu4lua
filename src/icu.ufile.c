
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unicode/ustdio.h>
#include <unicode/ustring.h>
#include <lua.h>
#include <lauxlib.h>
#include "icu4lua.h"

// All icu.ufile functions have these upvalues set
#define UFILE_UV_META			lua_upvalueindex(1)
#define UFILE_UV_USTRING_META	lua_upvalueindex(2)
#define UFILE_UV_USTRING_POOL	lua_upvalueindex(3)
#define UFILE_UV_ITER_UFILE		lua_upvalueindex(4)

// Utility functions - based on equivalents in liolib.c
static int read_line(lua_State *L, UFILE *ufile) {
	luaL_Buffer b;
	luaL_buffinit(L, &b);
	for (;;) {
		size_t l;
		UChar* p = icu4lua_prepubuffer(&b);
		if (u_fgets(p, ICU4LUA_UBUFFERSIZE, ufile) == NULL) {
			icu4lua_pushuresult(&b, UFILE_UV_USTRING_META, UFILE_UV_USTRING_POOL);
			if (icu4lua_ustrlen(L,-1) == 0) {
				return 0;
			}
			return 1;
		}
		l = u_strlen(p);
		if (l == 0 || p[l-1] != '\n') {
			icu4lua_addusize(&b, l);
		}
		else {
			icu4lua_addusize(&b, l - 1);
			icu4lua_pushuresult(&b, UFILE_UV_USTRING_META, UFILE_UV_USTRING_POOL);
			return 1;
		}
	}
}

static int read_chars(lua_State *L, UFILE *ufile, int32_t n) {
	int32_t rlen;  // how much to read
	int32_t nr;  // number of chars actually read
	luaL_Buffer b;
	luaL_buffinit(L, &b);
	rlen = ICU4LUA_UBUFFERSIZE;  // try to read that much each time
	do {
		UChar* p = icu4lua_prepubuffer(&b);
		if (rlen > n) {
			rlen = n;  // cannot read more than asked
		}
		nr = u_file_read(p, rlen, ufile);
		icu4lua_addusize(&b, nr);
		n -= nr;  // still have to read `n' chars
	}
	while (n > 0 && nr == rlen);  // until end of count or eof
	icu4lua_pushuresult(&b, UFILE_UV_USTRING_META, UFILE_UV_USTRING_POOL);
	if (icu4lua_ustrlen(L,-1) == 0 && n != 0) {
		return 0;
	}
	return 1;
}

static int read_number(lua_State *L, UFILE *ufile) {
	lua_Number d;
	if (u_fscanf(ufile, LUA_NUMBER_SCAN, &d) == 1) {
		lua_pushnumber(L, d);
		return 1;
	}
	else return 0;  /* read fails */
}

static int test_eof(lua_State *L, UFILE *ufile) {
	UChar32 c = u_fgetcx(ufile);
	u_fungetc(c, ufile);
	lua_pushlstring(L, NULL, 0);
	icu4lua_internrawustring(L, UFILE_UV_USTRING_META, UFILE_UV_USTRING_POOL);
	return (c != EOF);
}

static int pushresult(lua_State *L, int i, const char *filename) {
	int en = errno;  // calls to Lua API may change this value
	if (i) {
		lua_pushboolean(L, 1);
		return 1;
	}
	else {
		lua_pushnil(L);
		if (filename) {
			lua_pushfstring(L, "%s: %s", filename, strerror(en));
		}
		else {
			lua_pushfstring(L, "%s", strerror(en));
		}
		lua_pushinteger(L, en);
		return 3;
	}
}

// Actual library methods

static int icu_ufile_open(lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	const char *mode = luaL_optstring(L, 2, "r");
	UFILE* ufile;
	/*
	FILE* f = fopen(filename, mode);
	if (f == NULL) {
		return pushresult(L, 0, filename);
	}
	ufile = u_finit(f, luaL_optstring(L,4,NULL), luaL_optstring(L,3,NULL));
	*/
	ufile = u_fopen(filename, mode, luaL_optstring(L,4,NULL), luaL_optstring(L,3,NULL));
	if (ufile == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, "unable to initialize ufile");
		return 2;
	}
	*(UFILE**)lua_newuserdata(L, sizeof(UFILE*)) = ufile;
	lua_pushvalue(L, UFILE_UV_META);
	lua_setmetatable(L, -2);
	return 1;
}

static int icu_ufile_close(lua_State *L) {
	UFILE* ufile = icu4lua_checkopenufile(L,1,UFILE_UV_META);
	u_fclose(ufile);
	*(UFILE**)lua_touserdata(L,1) = NULL;
	lua_pushboolean(L,1);
	return 1;
}

static int icu_ufile_flush(lua_State *L) {
	u_fflush(icu4lua_checkopenufile(L,1,UFILE_UV_META));
	return 0;
}

static int lines_aux(lua_State *L) {
	UFILE* ufile = icu4lua_trustufile(L, UFILE_UV_ITER_UFILE);
	if (ufile == NULL) {
		return 0;
	}
	return read_line(L, ufile);
}

static int icu_ufile_lines(lua_State *L) {
	icu4lua_checkopenufile(L,1,UFILE_UV_META);
	lua_settop(L,1);
	lua_pushvalue(L, UFILE_UV_META);
	lua_pushvalue(L, UFILE_UV_USTRING_META);
	lua_pushvalue(L, UFILE_UV_USTRING_POOL);
	lua_pushvalue(L, 1);
	lua_pushcclosure(L, lines_aux, 4);
	return 1;
}

static int icu_ufile_read(lua_State *L) {
	int nargs = lua_gettop(L) - 1;
	int success;
	int n;
	UFILE* ufile = icu4lua_checkufile(L,1,UFILE_UV_META);
	if (nargs == 0) { // no arguments?
		success = read_line(L, ufile);
		n = 3; // to return 1 result
	}
	else {
		luaL_checkstack(L, nargs+LUA_MINSTACK, "too many arguments");
		success = 1;
		for (n = 2; nargs-- && success; n++) {
			if (lua_type(L,n) == LUA_TNUMBER) {
				int32_t l = (int32_t)lua_tointeger(L, n);
				success = (l == 0) ? test_eof(L, ufile) : read_chars(L, ufile, l);
			}
			else {
				const char* p = lua_tostring(L, n);
				luaL_argcheck(L, p && p[0] == '*', n, "invalid option");
				switch(p[1]) {
					case 'n': // number
						success = read_number(L, ufile);
						break;
					case 'l': // line
						success = read_line(L, ufile);
						break;
					case 'a': // file
						read_chars(L, ufile, INT32_MAX);
						success = 1;
						break;
					default:
						return luaL_argerror(L, n, "invalid format");
				}
			}
		}
	}
	if (!success) {
		lua_pop(L,1); // remove last result
		lua_pushnil(L); // push nil instead
	}
	return n - 2;
}

static int icu_ufile_rewind(lua_State *L) {
	u_frewind(icu4lua_checkufile(L, 1, UFILE_UV_META));
	return 0;
}

static int icu_ufile_write(lua_State *L) {
	int nargs = lua_gettop(L) - 1;
	int arg = 2;
	int status = 1;
	UFILE* ufile = icu4lua_checkopenufile(L,1,UFILE_UV_META);
	for (; nargs--; arg++) {
		if (lua_type(L, arg) == LUA_TNUMBER) {
			status = status && u_fprintf(ufile, LUA_NUMBER_FMT, lua_tonumber(L, arg))>0;
		}
		else {
			const UChar* ustr = icu4lua_checkustring(L, arg, UFILE_UV_USTRING_META);
			int32_t ustr_l = (int32_t)icu4lua_ustrlen(L, arg);
			status = status && (u_file_write(ustr, ustr_l, ufile) == ustr_l);
		}
	}
	return pushresult(L, status, NULL);
}

static int icu_ufile_locale(lua_State *L) {
	UFILE* ufile = icu4lua_checkopenufile(L,1,UFILE_UV_META);
	if (lua_gettop(L) == 1) {
		lua_pushstring(L, u_fgetlocale(ufile));
		return 1;
	}
	else {
		if (u_fsetlocale(ufile, luaL_optstring(L,2,NULL)) != 0) {
			return luaL_error(L, "unable to set ufile locale");
		}
		lua_settop(L,1);
		return 1;
	}
}

static int icu_ufile_encoding(lua_State *L) {
	UFILE* ufile = icu4lua_checkopenufile(L,1,UFILE_UV_META);
	if (lua_gettop(L) == 1) {
		lua_pushstring(L, u_fgetcodepage(ufile));
		return 1;
	}
	else {
		if (u_fsetcodepage(luaL_optstring(L,2,NULL), ufile) != 0) {
			return luaL_error(L, "unable to set ufile encoding");
		}
		lua_settop(L,1);
		return 1;
	}
}

static int icu_ufile__gc(lua_State *L) {
	UFILE* ufile = icu4lua_trustufile(L,1);
	if (ufile) {
		u_fclose(ufile);
	}
	return 0;
}

static int icu_ufile__tostring(lua_State *L) {
	UFILE* ufile = *(UFILE**)lua_touserdata(L,1);
	if (!ufile) {
		lua_pushliteral(L, "ufile (closed)");
	}
	else {
		lua_pushfstring(L, "ufile (%p)", ufile);
	}
	return 1;
}

static const luaL_Reg icu_ufile_lib[] = {
	{"open", icu_ufile_open},
	{"encoding", icu_ufile_encoding},
	{"locale", icu_ufile_locale},
	{"read", icu_ufile_read},
	{"write", icu_ufile_write},
	{"lines", icu_ufile_lines},
	{"rewind", icu_ufile_rewind},
	{"flush", icu_ufile_flush},
	{"close", icu_ufile_close},
	{NULL, NULL}
};

static const luaL_Reg icu_ufile_meta[] = {
	{"__gc", icu_ufile__gc},
	{"__tostring", icu_ufile__tostring},
	{NULL, NULL}
};

int luaopen_icu_ufile(lua_State *L) {
	int IDX_UFILE_META, IDX_UFILE_LIB, IDX_USTRING_META, IDX_USTRING_POOL;
	const luaL_Reg* lib_entry;
	static const luaL_Reg null_entry = {NULL,NULL};

	icu4lua_requireustringlib(L);

	icu4lua_pushustringmetatable(L);
	IDX_USTRING_META = lua_gettop(L);

	icu4lua_pushustringpool(L);
	IDX_USTRING_POOL = lua_gettop(L);
	
	luaL_newmetatable(L, "UFILE*");
	luaL_register(L, NULL, icu_ufile_meta);
	IDX_UFILE_META = lua_gettop(L);

	luaL_register(L, "icu.ufile", &null_entry);
	IDX_UFILE_LIB = lua_gettop(L);

	for (lib_entry = icu_ufile_lib; lib_entry->name; lib_entry++) {
		lua_pushvalue(L, IDX_UFILE_META);
		lua_pushvalue(L, IDX_USTRING_META);
		lua_pushvalue(L, IDX_USTRING_POOL);
		lua_pushnil(L);
		lua_pushcclosure(L, lib_entry->func, 4);
		lua_setfield(L, IDX_UFILE_LIB, lib_entry->name);
	}

	lua_pushvalue(L, IDX_UFILE_LIB);
	lua_setfield(L, IDX_UFILE_META, "__index");

	lua_settop(L, IDX_UFILE_LIB);
	return 1;
}
