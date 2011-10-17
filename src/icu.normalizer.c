
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <unicode/unorm.h>
#include "icu4lua.h"

// All icu.normalizer functions have these upvalues set
#define NORMALIZER_UV_USTRING_META	lua_upvalueindex(1)
#define NORMALIZER_UV_USTRING_POOL	lua_upvalueindex(2)

static const UNormalizationMode modes[] = {UNORM_NFD, UNORM_NFC, UNORM_NFKC, UNORM_NFKD, UNORM_FCD, UNORM_NONE};
static const char* modeNames[] = {"nfd", "nfc", "nfkc", "nfkd", "fcd", "none", NULL};
#define DEFAULT_MODE_OPTION	"nfc"

static int icu_normalizer_normalize(lua_State *L) {
	const UChar* source = icu4lua_checkustring(L, 1, NORMALIZER_UV_USTRING_META);
	int32_t sourceLength = (int32_t)icu4lua_ustrlen(L, 1);
	UNormalizationMode mode;
	UChar* result;
	int32_t resultLength;
	UErrorCode status;
	int32_t options;

	mode = modes[luaL_checkoption(L, 2, DEFAULT_MODE_OPTION, modeNames)];
	options = (int32_t)luaL_optnumber(L,3,0);

	status = U_ZERO_ERROR;
	resultLength = unorm_normalize(source, sourceLength, mode, options, NULL, 0, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}

	result = (UChar*)malloc(sizeof(UChar) * resultLength);
	status = U_ZERO_ERROR;
	unorm_normalize(source, sourceLength, mode, options, result, resultLength, &status);
	if (U_FAILURE(status)) {
		free(result);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}

	icu4lua_pushustring(L, result, resultLength, NORMALIZER_UV_USTRING_META, NORMALIZER_UV_USTRING_POOL);
	free(result);
	return 1;
}

static int icu_normalizer_quickcheck(lua_State *L) {
	UNormalizationMode mode;
	UErrorCode status;
	UNormalizationCheckResult result;
	luaL_argcheck(L, lua_getmetatable(L,1) && lua_rawequal(L,-1,NORMALIZER_UV_USTRING_META), 1, "expecting ustring");
	lua_pop(L,1);
	mode = modes[luaL_checkoption(L, 2, DEFAULT_MODE_OPTION, modeNames)];
	status = U_ZERO_ERROR;
	result = unorm_quickCheckWithOptions(
		icu4lua_trustustring(L,1),
		(int32_t)icu4lua_ustrlen(L,1),
		mode,
		(int32_t)luaL_optnumber(L,3,0),
		&status);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	switch(result) {
		case UNORM_YES:
			lua_pushliteral(L, "yes");
			break;
		case UNORM_NO:
			lua_pushliteral(L, "no");
			break;
		case UNORM_MAYBE:
		default:
			lua_pushliteral(L, "maybe");
			break;
	}
	return 1;
}

static int icu_normalizer_isnormalized(lua_State *L) {
	UNormalizationMode mode;
	UErrorCode status;
	
	icu4lua_checkustring(L,1,NORMALIZER_UV_USTRING_META);
	mode = modes[luaL_checkoption(L,2,DEFAULT_MODE_OPTION,modeNames)];

	status = U_ZERO_ERROR;
	lua_pushboolean(L,
		unorm_isNormalized(icu4lua_trustustring(L,1), (int32_t)icu4lua_ustrlen(L,1), mode, &status));
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static int icu_normalizer_concat(lua_State *L) {
	const UChar* left = icu4lua_checkustring(L,1,NORMALIZER_UV_USTRING_META);
	int32_t left_length = (int32_t)icu4lua_ustrlen(L,1);
	const UChar* right = icu4lua_checkustring(L,2,NORMALIZER_UV_USTRING_META);
	int32_t right_length = (int32_t)icu4lua_ustrlen(L,2);
	UNormalizationMode mode = modes[luaL_checkoption(L,3,DEFAULT_MODE_OPTION, modeNames)];
	int32_t options = luaL_optint(L,4,0);
	UChar* dest;
	int32_t dest_length;
	UErrorCode status = U_ZERO_ERROR;
	dest_length = unorm_concatenate(left, left_length, right, right_length, NULL, 0, mode, options, &status);
	if (U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	dest = (UChar*)malloc(sizeof(UChar) * dest_length);
	status = U_ZERO_ERROR;
	unorm_concatenate(left, left_length, right, right_length, dest, dest_length, mode, options, &status);
	if (U_FAILURE(status)) {
		free(dest);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	icu4lua_pushustring(L, dest, dest_length, NORMALIZER_UV_USTRING_META, NORMALIZER_UV_USTRING_POOL);
	return 1;
}

static int icu_normalizer_equals(lua_State *L) {
	const UChar* left = icu4lua_checkustring(L,1,NORMALIZER_UV_USTRING_META);
	int32_t left_length = (int32_t)icu4lua_ustrlen(L,1);
	const UChar* right = icu4lua_checkustring(L,2,NORMALIZER_UV_USTRING_META);
	int32_t right_length = (int32_t)icu4lua_ustrlen(L,2);
	int32_t options = luaL_optint(L,3,0);
	UErrorCode status = U_ZERO_ERROR;
	lua_pushboolean(L, unorm_compare(left, left_length, right, right_length, options, &status) == 0);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static int icu_normalizer_lessthan(lua_State *L) {
	const UChar* left = icu4lua_checkustring(L,1,NORMALIZER_UV_USTRING_META);
	int32_t left_length = (int32_t)icu4lua_ustrlen(L,1);
	const UChar* right = icu4lua_checkustring(L,2,NORMALIZER_UV_USTRING_META);
	int32_t right_length = (int32_t)icu4lua_ustrlen(L,2);
	int32_t options = luaL_optint(L,3,0);
	UErrorCode status = U_ZERO_ERROR;
	lua_pushboolean(L, unorm_compare(left, left_length, right, right_length, options, &status) < 0);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static int icu_normalizer_lessorequal(lua_State *L) {
	const UChar* left = icu4lua_checkustring(L,1,NORMALIZER_UV_USTRING_META);
	int32_t left_length = (int32_t)icu4lua_ustrlen(L,1);
	const UChar* right = icu4lua_checkustring(L,2,NORMALIZER_UV_USTRING_META);
	int32_t right_length = (int32_t)icu4lua_ustrlen(L,2);
	int32_t options = luaL_optint(L,3,0);
	UErrorCode status = U_ZERO_ERROR;
	lua_pushboolean(L, unorm_compare(left, left_length, right, right_length, options, &status) <= 0);
	if (U_FAILURE(status)) {
		lua_pushstring(L, u_errorName(status));
		return lua_error(L);
	}
	return 1;
}

static const luaL_Reg icu_normalizer_lib[] = {
	{"normalize", icu_normalizer_normalize},
	{"quickcheck", icu_normalizer_quickcheck},
	{"isnormalized", icu_normalizer_isnormalized},
	{"concat", icu_normalizer_concat},
	{"equals", icu_normalizer_equals},
	{"lessthan", icu_normalizer_lessthan},
	{"lessorequal", icu_normalizer_lessorequal},
	{NULL, NULL}
};

typedef struct normalizer_constant {
	const char* name;
	int32_t value;
} normalizer_constant;

static const normalizer_constant normalizer_constants[] = {
	{"INPUT_IS_FCD", UNORM_INPUT_IS_FCD},
	{"COMPARE_IGNORE_CASE", U_COMPARE_IGNORE_CASE},
	{"COMPARE_CODE_POINT_ORDER", U_COMPARE_CODE_POINT_ORDER},
	{NULL, 0}
};

int luaopen_icu_normalizer(lua_State *L) {
	int IDX_NORMALIZER_LIB, IDX_USTRING_META, IDX_USTRING_POOL;
	static const luaL_Reg null_entry = {NULL,NULL};
	const luaL_Reg* lib_entry;
	const normalizer_constant* lib_constant;

	icu4lua_requireustringlib(L);

	icu4lua_pushustringmetatable(L);
	IDX_USTRING_META = lua_gettop(L);

	icu4lua_pushustringpool(L);
	IDX_USTRING_POOL = lua_gettop(L);

	luaL_register(L, "icu.normalizer", &null_entry);
	IDX_NORMALIZER_LIB = lua_gettop(L);
	for (lib_entry = icu_normalizer_lib; lib_entry->name; lib_entry++) {
		lua_pushvalue(L, IDX_USTRING_META);
		lua_pushvalue(L, IDX_USTRING_POOL);
		lua_pushcclosure(L, lib_entry->func, 2);
		lua_setfield(L, IDX_NORMALIZER_LIB, lib_entry->name);
	}
	for (lib_constant = normalizer_constants; lib_constant->name; lib_constant++) {
		lua_pushnumber(L, lib_constant->value);
		lua_setfield(L, IDX_NORMALIZER_LIB, lib_constant->name);
	}

	lua_settop(L, IDX_NORMALIZER_LIB);
	return 1;
}
