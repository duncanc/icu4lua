
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <unicode/usprep.h>
#include "icu4lua.h"

// All icu.stringprep functions have these upvalues set
#define STRINGPREP_UV_META			lua_upvalueindex(1)
#define STRINGPREP_UV_USTRING_META	lua_upvalueindex(2)
#define STRINGPREP_UV_USTRING_POOL	lua_upvalueindex(3)

typedef struct stringPrepConstant {
	const char* name;
	int value;
} stringPrepConstant;

static int icu_stringprep_open(lua_State *L) {
	UErrorCode status = U_ZERO_ERROR;
	UStringPrepProfile* profile = usprep_open(luaL_checkstring(L,1), luaL_checkstring(L,2), &status);
	if (U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	*(UStringPrepProfile**)lua_newuserdata(L, sizeof(UStringPrepProfile*)) = profile;
	lua_pushvalue(L, STRINGPREP_UV_META);
	lua_setmetatable(L, -2);
	return 1;
}

static int icu_stringprep_openbytype(lua_State *L) {
	UErrorCode status = U_ZERO_ERROR;
	UStringPrepProfile* profile = usprep_openByType(luaL_checknumber(L,1), &status);
	if (U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	*(UStringPrepProfile**)lua_newuserdata(L, sizeof(UStringPrepProfile*)) = profile;
	lua_pushvalue(L, STRINGPREP_UV_META);
	lua_setmetatable(L, -2);
	return 1;
}

static int icu_stringprep_close(lua_State *L) {
	luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,STRINGPREP_UV_META), 1, "expecting stringprep profile");
	lua_pop(L,1);
	usprep_close(*(UStringPrepProfile**)lua_touserdata(L,1));
	lua_pushnil(L);
	lua_setmetatable(L,1);
	return 0;
}

static int icu_stringprep_prepare(lua_State *L) {
	UStringPrepProfile* profile;
	const UChar* src;
	int32_t src_len;
	int options;
	UChar* dest;
	int dest_len;
	UParseError parseError;
	UErrorCode status;

	luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,STRINGPREP_UV_META), 1, "expecting stringprep profile");
	lua_pop(L,1);
	profile = *(UStringPrepProfile**)lua_touserdata(L,1);

	src = icu4lua_checkustring(L,2,STRINGPREP_UV_USTRING_META);
	src_len = (int32_t)icu4lua_ustrlen(L,2);

	options = (int)luaL_optnumber(L, 3, USPREP_DEFAULT);

	status = U_ZERO_ERROR;
	dest_len = usprep_prepare(profile, src, src_len, NULL, 0, options, &parseError, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushnumber(L, parseError.line);
		lua_pushnumber(L, parseError.offset);
		return 4;
	}
	dest = (UChar*)malloc(sizeof(UChar) * dest_len);
	status = U_ZERO_ERROR;
	usprep_prepare(profile, src, src_len, dest, dest_len, options, &parseError, &status);
	if (U_FAILURE(status)) {
		dest = NULL;
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		lua_pushnumber(L, parseError.line);
		lua_pushnumber(L, parseError.offset);
		return 4;
	}
	icu4lua_pushustring(L, dest, dest_len, STRINGPREP_UV_USTRING_META, STRINGPREP_UV_USTRING_POOL);
	return 1;
}

static const luaL_Reg icu_stringprep_lib[] = {
	{"open", icu_stringprep_open},
	{"openbytype", icu_stringprep_openbytype},
	{"close", icu_stringprep_close},
	{"prepare", icu_stringprep_prepare},
	{NULL, NULL}
};

static const stringPrepConstant icu_stringprep_lib_constants[] = {
	{"RFC3491_NAMEPREP", USPREP_RFC3491_NAMEPREP},
	{"RFC3530_NFS4_CS_PREP", USPREP_RFC3530_NFS4_CS_PREP},
	{"RFC3530_NFS4_CS_PREP_CI", USPREP_RFC3530_NFS4_CS_PREP_CI},
	{"RFC3530_NFS4_CIS_PREP", USPREP_RFC3530_NFS4_CIS_PREP},
	{"RFC3530_NFS4_MIXED_PREP_PREFIX", USPREP_RFC3530_NFS4_MIXED_PREP_PREFIX},
	{"RFC3530_NFS4_MIXED_PREP_SUFFIX", USPREP_RFC3530_NFS4_MIXED_PREP_SUFFIX},
	{"RFC3722_ISCSI", USPREP_RFC3722_ISCSI},
	{"RFC3920_NODEPREP", USPREP_RFC3920_NODEPREP},
	{"RFC3920_RESOURCEPREP", USPREP_RFC3920_RESOURCEPREP},
	{"RFC4011_MIB", USPREP_RFC4011_MIB},
	{"RFC4013_SASLPREP", USPREP_RFC4013_SASLPREP},
	{"RFC4505_TRACE", USPREP_RFC4505_TRACE},
	{"RFC4518_LDAP", USPREP_RFC4518_LDAP},
	{"RFC4518_LDAP_CI", USPREP_RFC4518_LDAP_CI},

	{"DEFAULT", USPREP_DEFAULT},
	{"ALLOW_UNASSIGNED", USPREP_ALLOW_UNASSIGNED},

	{NULL,0}
};

static int icu_stringprep__gc(lua_State *L) {
	usprep_close(*(UStringPrepProfile**)lua_touserdata(L,1));
	return 0;
}

static int icu_stringprep__tostring(lua_State *L) {
	lua_pushfstring(L, "icu.stringprep profile: %p", *(UStringPrepProfile**)lua_touserdata(L,1));
	return 1;
}

static const luaL_Reg icu_stringprep_meta[] = {
	{"__gc", icu_stringprep__gc},
	{"__tostring", icu_stringprep__tostring}
};

int luaopen_icu_stringprep(lua_State *L) {
	int IDX_STRINGPREP_LIB, IDX_STRINGPREP_META, IDX_USTRING_META, IDX_USTRING_POOL;
	const stringPrepConstant* lib_constant;
	const luaL_Reg null_entry = {NULL,NULL};
	const luaL_Reg* lib_entry;

	icu4lua_requireustringlib(L);

	icu4lua_pushustringmetatable(L);
	IDX_USTRING_META = lua_gettop(L);
	
	icu4lua_pushustringpool(L);
	IDX_USTRING_POOL = lua_gettop(L);

	luaL_newmetatable(L, "icu.stringprep");
	luaL_register(L, NULL, icu_stringprep_meta);
	IDX_STRINGPREP_META = lua_gettop(L);

	luaL_register(L, "icu.stringprep", &null_entry);
	IDX_STRINGPREP_LIB = lua_gettop(L);
	for (lib_entry = icu_stringprep_lib; lib_entry->name; lib_entry++) {
		lua_pushstring(L, lib_entry->name);
		lua_pushvalue(L, IDX_STRINGPREP_META);
		lua_pushvalue(L, IDX_USTRING_META);
		lua_pushvalue(L, IDX_USTRING_POOL);
		lua_pushcclosure(L, lib_entry->func, 3);
		lua_rawset(L, IDX_STRINGPREP_LIB);
	}

	lua_pushvalue(L, IDX_STRINGPREP_LIB);
	lua_setfield(L, IDX_STRINGPREP_META, "__index");

	for (lib_constant = icu_stringprep_lib_constants; lib_constant->name; lib_constant++) {
		lua_pushstring(L, lib_constant->name);
		lua_pushnumber(L, lib_constant->value);
		lua_rawset(L, IDX_STRINGPREP_LIB);
	}

	lua_settop(L, IDX_STRINGPREP_LIB);
	return 1;
}
