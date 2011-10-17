
#include <lua.h>
#include <lauxlib.h>
#include <unicode/ucol.h>
#include "icu4lua.h"

// All icu.collator functions have these upvalues set
#define COLLATOR_UV_META			lua_upvalueindex(1)
#define COLLATOR_UV_USTRING_META	lua_upvalueindex(2)
#define COLLATOR_UV_USTRING_POOL	lua_upvalueindex(3)

static int icu_collator_open(lua_State *L) {
	UErrorCode status = U_ZERO_ERROR;
	UCollator* new_col = ucol_open(luaL_checkstring(L,1), &status);
	if (U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	*(UCollator**)lua_newuserdata(L, sizeof(UCollator*)) = new_col;
	lua_pushvalue(L, COLLATOR_UV_META);
	lua_setmetatable(L, -2);
	return 1;
}

static int icu_collator_equals(lua_State *L) {
	luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,COLLATOR_UV_META), 1, "expecting collator");
	lua_pop(L,1);
	icu4lua_checkustring(L,2,COLLATOR_UV_USTRING_META);
	icu4lua_checkustring(L,3,COLLATOR_UV_USTRING_META);
	lua_pushboolean(L, ucol_equal(
		*(UCollator**)lua_touserdata(L,1),
		icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
		icu4lua_trustustring(L,3), (int32_t)icu4lua_ustrlen(L,3)));
	return 1;
}

static int icu_collator_lessthan(lua_State *L) {
	luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,COLLATOR_UV_META), 1, "expecting collator");
	lua_pop(L,1);
	icu4lua_checkustring(L,2,COLLATOR_UV_USTRING_META);
	icu4lua_checkustring(L,3,COLLATOR_UV_USTRING_META);
	lua_pushboolean(L, !ucol_greaterOrEqual(
		*(UCollator**)lua_touserdata(L,1),
		icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
		icu4lua_trustustring(L,3), (int32_t)icu4lua_ustrlen(L,3)));
	return 1;
}

static int icu_collator_lessorequal(lua_State *L) {
	luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,COLLATOR_UV_META), 1, "expecting collator");
	lua_pop(L,1);
	icu4lua_checkustring(L,2,COLLATOR_UV_USTRING_META);
	icu4lua_checkustring(L,3,COLLATOR_UV_USTRING_META);
	lua_pushboolean(L, !ucol_greater(
		*(UCollator**)lua_touserdata(L,1),
		icu4lua_trustustring(L,2), (int32_t)icu4lua_ustrlen(L,2),
		icu4lua_trustustring(L,3), (int32_t)icu4lua_ustrlen(L,3)));
	return 1;
}

static int icu_collator_strength(lua_State *L) {
	luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,COLLATOR_UV_META), 1, "expecting collator");
	lua_pop(L,1);
	if (lua_gettop(L) == 1) {
		lua_pushnumber(L, ucol_getStrength(*(UCollator**)lua_touserdata(L,1)));
		return 1;
	}
	else {
		ucol_setStrength(*(UCollator**)lua_touserdata(L,1), (UCollationStrength)luaL_checknumber(L,2));
		lua_settop(L,1);
		return 1;
	}
}

static const luaL_Reg icu_collator_lib[] = {
	{"open", icu_collator_open},

	{"equals", icu_collator_equals},
	{"lessthan", icu_collator_lessthan},
	{"lessorequal", icu_collator_lessorequal},

	{"strength", icu_collator_strength},
	{NULL, NULL}
};

static int icu_collator__gc(lua_State *L) {
	ucol_close(*(UCollator**)lua_touserdata(L,1));
	return 0;
}

static int icu_collator__tostring(lua_State *L) {
	lua_pushfstring(L, "icu.collator: %p", *(UCollator**)lua_touserdata(L,1));
	return 1;
}

static const luaL_Reg icu_collator_meta[] = {
	{"__gc", icu_collator__gc},
	{"__tostring", icu_collator__tostring},
	{NULL, NULL}
};

typedef struct collatorConstant {
	const char* name;
	int value;
} collatorConstant;

static const collatorConstant icu_collator_lib_constants[] = {
	{"EQUAL", UCOL_EQUAL},
	{"GREATER", UCOL_GREATER},
	{"LESS", UCOL_LESS},
	{"DEFAULT", UCOL_DEFAULT},
	{"PRIMARY", UCOL_PRIMARY},
	{"SECONDARY", UCOL_SECONDARY},
	{"TERTIARY", UCOL_TERTIARY},
	{"DEFAULT_STRENGTH", UCOL_DEFAULT_STRENGTH},
	{"CE_STRENGTH_LIMIT", UCOL_CE_STRENGTH_LIMIT},
	{"QUATERNARY", UCOL_QUATERNARY},
	{"IDENTICAL", UCOL_IDENTICAL},
	{"STRENGTH_LIMIT", UCOL_STRENGTH_LIMIT},
	{"OFF", UCOL_OFF},
	{"ON", UCOL_ON},
	{"SHIFTED", UCOL_SHIFTED},
	{"NON_IGNORABLE", UCOL_NON_IGNORABLE},
	{"LOWER_FIRST", UCOL_LOWER_FIRST},
	{"UPPER_FIRST", UCOL_UPPER_FIRST},
	{"FRENCH_COLLATION", UCOL_FRENCH_COLLATION},
	{"ALTERNATE_HANDLING", UCOL_ALTERNATE_HANDLING},
	{"CASE_FIRST", UCOL_CASE_FIRST},
	{"CASE_LEVEL", UCOL_CASE_LEVEL},
	{"NORMALIZATION_MODE", UCOL_NORMALIZATION_MODE},
	{"DECOMPOSITION_MODE", UCOL_DECOMPOSITION_MODE},
	{"STRENGTH", UCOL_STRENGTH},
	{"HIRAGANA_QUATERNARY_MODE", UCOL_HIRAGANA_QUATERNARY_MODE},
	{"NUMERIC_COLLATION", UCOL_NUMERIC_COLLATION},
	{"TAILORING_ONLY", UCOL_TAILORING_ONLY},
	{"FULL_RULES", UCOL_FULL_RULES},
	{"BOUND_LOWER", UCOL_BOUND_LOWER},
	{"BOUND_UPPER", UCOL_BOUND_UPPER},
	{"BOUND_UPPER_LONG", UCOL_BOUND_UPPER_LONG},
	{NULL, 0}
};

int luaopen_icu_collator(lua_State *L) {
	int IDX_COLLATOR_META, IDX_COLLATOR_LIB, IDX_USTRING_META, IDX_USTRING_POOL;
	const luaL_Reg* lib_entry;
	const luaL_Reg null_entry = {NULL,NULL};
	const collatorConstant* lib_constant;

	icu4lua_requireustringlib(L);

	luaL_newmetatable(L, "icu.collator");
	luaL_register(L, NULL, icu_collator_meta);
	IDX_COLLATOR_META = lua_gettop(L);

	icu4lua_pushustringmetatable(L);
	IDX_USTRING_META = lua_gettop(L);

	icu4lua_pushustringpool(L);
	IDX_USTRING_POOL = lua_gettop(L);

	luaL_register(L, "icu.collator", &null_entry);
	IDX_COLLATOR_LIB = lua_gettop(L);
	for (lib_entry = icu_collator_lib; lib_entry->name; lib_entry++) {
		lua_pushstring(L, lib_entry->name);
		lua_pushvalue(L, IDX_COLLATOR_META);
		lua_pushvalue(L, IDX_USTRING_META);
		lua_pushvalue(L, IDX_USTRING_POOL);
		lua_pushcclosure(L, lib_entry->func, 3);
		lua_rawset(L, IDX_COLLATOR_LIB);
	}
	for (lib_constant = icu_collator_lib_constants; lib_constant->name; lib_constant++) {
		lua_pushstring(L, lib_constant->name);
		lua_pushnumber(L, lib_constant->value);
		lua_rawset(L, IDX_COLLATOR_LIB);
	}
	
	lua_pushvalue(L, IDX_COLLATOR_LIB);
	lua_setfield(L, IDX_COLLATOR_META, "__index");

	lua_settop(L, IDX_COLLATOR_LIB);
	return 1;
}
