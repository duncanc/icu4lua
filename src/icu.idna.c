
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <unicode/uidna.h>
#include "icu4lua.h"

// All icu.idna functions have these upvalues set
#define IDNA_UV_USTRING_META	lua_upvalueindex(1)
#define IDNA_UV_USTRING_POOL	lua_upvalueindex(2)

static int icu_idna_toascii(lua_State *L) {
	const UChar* ustring = icu4lua_checkustring(L,1,IDNA_UV_USTRING_META);
	int32_t ustring_len = (int32_t)icu4lua_ustrlen(L,1);
	int32_t options = luaL_optint(L,2,UIDNA_DEFAULT);
	UChar* new_ustring;
	int32_t new_ustring_len;
	UErrorCode status;
	UParseError parseError;

	status = U_ZERO_ERROR;
	new_ustring_len = uidna_toASCII(ustring, ustring_len, NULL, 0, options, &parseError, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushinteger(L, parseError.line);
		lua_pushinteger(L, parseError.offset);
		return 4;
	}
	new_ustring = (UChar*)malloc(sizeof(UChar) * new_ustring_len);
	status = U_ZERO_ERROR;
	uidna_toASCII(ustring, ustring_len, new_ustring, new_ustring_len, options, &parseError, &status);
	if (U_FAILURE(status)) {
		free(new_ustring);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushinteger(L, parseError.line);
		lua_pushinteger(L, parseError.offset);
		return 4;
	}
	icu4lua_pushustring(L, new_ustring, new_ustring_len, IDNA_UV_USTRING_META, IDNA_UV_USTRING_POOL);
	free(new_ustring);
	return 1;
}

static int icu_idna_tounicode(lua_State *L) {
	const UChar* ustring = icu4lua_checkustring(L,1,IDNA_UV_USTRING_META);
	int32_t ustring_len = (int32_t)icu4lua_ustrlen(L,1);
	int32_t options = luaL_optint(L,2,UIDNA_DEFAULT);
	UChar* new_ustring;
	int32_t new_ustring_len;
	UErrorCode status;
	UParseError parseError;

	status = U_ZERO_ERROR;
	new_ustring_len = uidna_toUnicode(ustring, ustring_len, NULL, 0, options, &parseError, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushinteger(L, parseError.line);
		lua_pushinteger(L, parseError.offset);
		return 4;
	}
	new_ustring = (UChar*)malloc(sizeof(UChar) * new_ustring_len);
	status = U_ZERO_ERROR;
	uidna_toUnicode(ustring, ustring_len, new_ustring, new_ustring_len, options, &parseError, &status);
	if (U_FAILURE(status)) {
		free(new_ustring);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushinteger(L, parseError.line);
		lua_pushinteger(L, parseError.offset);
		return 4;
	}
	icu4lua_pushustring(L, new_ustring, new_ustring_len, IDNA_UV_USTRING_META, IDNA_UV_USTRING_POOL);
	free(new_ustring);
	return 1;
}

static int icu_idna_idntoascii(lua_State *L) {
	const UChar* ustring = icu4lua_checkustring(L,1,IDNA_UV_USTRING_META);
	int32_t ustring_len = (int32_t)icu4lua_ustrlen(L,1);
	int32_t options = luaL_optint(L,2,UIDNA_DEFAULT);
	UChar* new_ustring;
	int32_t new_ustring_len;
	UErrorCode status;
	UParseError parseError;

	status = U_ZERO_ERROR;
	new_ustring_len = uidna_IDNToASCII(ustring, ustring_len, NULL, 0, options, &parseError, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushinteger(L, parseError.line);
		lua_pushinteger(L, parseError.offset);
		return 4;
	}
	new_ustring = (UChar*)malloc(sizeof(UChar) * new_ustring_len);
	status = U_ZERO_ERROR;
	uidna_IDNToASCII(ustring, ustring_len, new_ustring, new_ustring_len, options, &parseError, &status);
	if (U_FAILURE(status)) {
		free(new_ustring);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushinteger(L, parseError.line);
		lua_pushinteger(L, parseError.offset);
		return 4;
	}
	icu4lua_pushustring(L, new_ustring, new_ustring_len, IDNA_UV_USTRING_META, IDNA_UV_USTRING_POOL);
	free(new_ustring);
	return 1;
}

static int icu_idna_idntounicode(lua_State *L) {
	const UChar* ustring = icu4lua_checkustring(L,1,IDNA_UV_USTRING_META);
	int32_t ustring_len = (int32_t)icu4lua_ustrlen(L,1);
	int32_t options = luaL_optint(L,2,UIDNA_DEFAULT);
	UChar* new_ustring;
	int32_t new_ustring_len;
	UErrorCode status;
	UParseError parseError;

	status = U_ZERO_ERROR;
	new_ustring_len = uidna_IDNToUnicode(ustring, ustring_len, NULL, 0, options, &parseError, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushinteger(L, parseError.line);
		lua_pushinteger(L, parseError.offset);
		return 4;
	}
	new_ustring = (UChar*)malloc(sizeof(UChar) * new_ustring_len);
	status = U_ZERO_ERROR;
	uidna_IDNToUnicode(ustring, ustring_len, new_ustring, new_ustring_len, options, &parseError, &status);
	if (U_FAILURE(status)) {
		free(new_ustring);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushinteger(L, parseError.line);
		lua_pushinteger(L, parseError.offset);
		return 4;
	}
	icu4lua_pushustring(L, new_ustring, new_ustring_len, IDNA_UV_USTRING_META, IDNA_UV_USTRING_POOL);
	free(new_ustring);
	return 1;
}

static int icu_idna_equals(lua_State *L) {
	const UChar* ustring_a = icu4lua_checkustring(L,1,IDNA_UV_USTRING_META);
	int32_t ustring_a_len = (int32_t)icu4lua_ustrlen(L,1);
	const UChar* ustring_b = icu4lua_checkustring(L,2,IDNA_UV_USTRING_META);
	int32_t ustring_b_len = (int32_t)icu4lua_ustrlen(L,2);
	int32_t options = luaL_optint(L,3,UIDNA_DEFAULT);
	UErrorCode status;

	status = U_ZERO_ERROR;
	lua_pushboolean(L, uidna_compare(ustring_a, ustring_a_len, ustring_b, ustring_b_len, options, &status) == 0);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static int icu_idna_lessthan(lua_State *L) {
	const UChar* ustring_a = icu4lua_checkustring(L,1,IDNA_UV_USTRING_META);
	int32_t ustring_a_len = (int32_t)icu4lua_ustrlen(L,1);
	const UChar* ustring_b = icu4lua_checkustring(L,2,IDNA_UV_USTRING_META);
	int32_t ustring_b_len = (int32_t)icu4lua_ustrlen(L,2);
	int32_t options = luaL_optint(L,3,UIDNA_DEFAULT);
	UErrorCode status;

	status = U_ZERO_ERROR;
	lua_pushboolean(L, uidna_compare(ustring_a, ustring_a_len, ustring_b, ustring_b_len, options, &status) < 0);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static int icu_idna_lessorequal(lua_State *L) {
	const UChar* ustring_a = icu4lua_checkustring(L,1,IDNA_UV_USTRING_META);
	int32_t ustring_a_len = (int32_t)icu4lua_ustrlen(L,1);
	const UChar* ustring_b = icu4lua_checkustring(L,2,IDNA_UV_USTRING_META);
	int32_t ustring_b_len = (int32_t)icu4lua_ustrlen(L,2);
	int32_t options = luaL_optint(L,3,UIDNA_DEFAULT);
	UErrorCode status;

	status = U_ZERO_ERROR;
	lua_pushboolean(L, uidna_compare(ustring_a, ustring_a_len, ustring_b, ustring_b_len, options, &status) <= 0);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static const luaL_Reg icu_idna_lib[] = {
	{"toascii", icu_idna_toascii},
	{"tounicode", icu_idna_tounicode},
	{"idntoascii", icu_idna_idntoascii},
	{"idntounicode", icu_idna_idntounicode},
	{"lessthan", icu_idna_lessthan},
	{"equals", icu_idna_equals},
	{"lessorequal", icu_idna_lessorequal},
	{NULL,NULL}
};

typedef struct icuIdnaConstant {
	const char* name;
	uint32_t value;
} icuIdnaConstant;

const static icuIdnaConstant icu_idna_constants[] = {
	{"DEFAULT", UIDNA_DEFAULT},
	{"ALLOW_UNASSIGNED", UIDNA_ALLOW_UNASSIGNED},
	{"USE_STD3_RULES", UIDNA_USE_STD3_RULES},
	{NULL, 0}
};

int luaopen_icu_idna(lua_State *L) {
	int IDX_USTRING_META, IDX_USTRING_POOL, IDX_IDNA_LIB;
	const luaL_Reg* lib_entry;
	const luaL_Reg null_entry = {NULL, NULL};

	icu4lua_requireustringlib(L);

	icu4lua_pushustringmetatable(L);
	IDX_USTRING_META = lua_gettop(L);

	icu4lua_pushustringpool(L);
	IDX_USTRING_POOL = lua_gettop(L);

	luaL_register(L, "icu.idna", &null_entry);
	IDX_IDNA_LIB = lua_gettop(L);
	for (lib_entry = icu_idna_lib; lib_entry->name; lib_entry++) {
		lua_pushstring(L, lib_entry->name);
		lua_pushvalue(L, IDX_USTRING_META);
		lua_pushvalue(L, IDX_USTRING_POOL);
		lua_pushcclosure(L, lib_entry->func, 2);
		lua_rawset(L, IDX_IDNA_LIB);
	}
	return 1;
}
