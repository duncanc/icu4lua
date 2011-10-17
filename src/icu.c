
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <unicode/ustring.h>
#include <unicode/uversion.h>
#include <unicode/ucnv.h>
#include <unicode/uloc.h>

static int icu_convert(lua_State *L) {
	UErrorCode status;
	size_t text_length;
	const char* text = luaL_checklstring(L,1,&text_length);
	char* new_text;
	int32_t new_text_length;
	const char* current_encoding = luaL_optstring(L,2,ucnv_getDefaultName());
	const char* new_encoding = luaL_optstring(L,3,ucnv_getDefaultName());
	status = U_ZERO_ERROR;
	new_text_length = ucnv_convert(new_encoding, current_encoding, NULL, 0, text, (int32_t)text_length, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	new_text = malloc(new_text_length);
	status = U_ZERO_ERROR;
	ucnv_convert(new_encoding, current_encoding, new_text, (int32_t)new_text_length, text, (int32_t)text_length, &status);
	if (U_FAILURE(status)) {
		free(new_text);
		lua_pushnil(L);
		lua_pushstring(L, u_errorName(status));
		return 2;
	}
	lua_pushlstring(L, new_text, new_text_length);
	free(new_text);
	return 1;
}

static int icu_defaultencoding(lua_State *L) {
	if (lua_gettop(L) != 0) {
		ucnv_setDefaultName(luaL_checkstring(L,1));
	}
	lua_pushstring(L, ucnv_getDefaultName());
	return 1;
}

static int icu_defaultlocale(lua_State *L) {
	if (lua_gettop(L) != 0) {
		UErrorCode status = U_ZERO_ERROR;
		uloc_setDefault(luaL_checkstring(L,1), &status);
	}
	lua_pushstring(L, uloc_getDefault());
	return 1;
}

static const luaL_Reg icu_lib[] = {
	{"convert", icu_convert},
	{"defaultencoding", icu_defaultencoding},
	{"defaultlocale", icu_defaultlocale},
	{NULL, NULL}
};

int luaopen_icu(lua_State *L) {
	luaL_register(L, "icu", icu_lib);

	lua_pushstring(L, U_ICU_VERSION);
	lua_setfield(L,-2,"_VERSION");

	return 1;
}
