
#include <string.h>

// Useful macros for dealing with ustrings
// meta_idx and pool_idx indices must be positive!

#define icu4lua_requireustringlib(L)																	\
	{																									\
		lua_getglobal(L, "require");																	\
		if (!lua_isfunction(L,-1)) {																	\
			lua_pushliteral(L, "library depends on the \"require\" function, which is missing");		\
			lua_error(L);																				\
		}																								\
		lua_pushliteral(L, "icu.ustring");																\
		lua_call(L,1,0);																				\
	}
#define icu4lua_pushustringmetatable(L)	(lua_getfield((L),LUA_REGISTRYINDEX,"icu.ustring"))
#define icu4lua_pushustringpool(L)		(lua_getfield((L),LUA_REGISTRYINDEX,"icu.ustring pool"))

#define icu4lua_ustrlen(L,i)			(lua_objlen((L),(i)) / sizeof(UChar))

#define icu4lua_pushustring(L,ustr,ustr_len,meta_idx,pool_idx)    										\
	{																									\
		lua_pushlstring((L), (const char*)(ustr), (ustr_len) * sizeof(UChar));							\
		icu4lua_internrawustring((L),(meta_idx),(pool_idx));											\
	}

#define icu4lua_checkustring(L,i,meta_idx)																\
	(																									\
		luaL_argcheck(																					\
			(L),																						\
			(lua_getmetatable((L),(i)) && lua_rawequal((L),-1,(meta_idx)) && (lua_pop(L,1),1)),			\
			(i),																						\
			"expecting ustring"																			\
		),																								\
		icu4lua_trustustring((L),(i))																	\
	)

#define icu4lua_internrawustring(L,meta_idx,pool_idx)													\
	{																									\
		lua_pushvalue((L),-1);																			\
		lua_rawget((L), (pool_idx));																	\
		if (lua_isnil((L),-1)) {																		\
			UChar* ___new_ustring;																		\
			lua_pop((L),1);																				\
			___new_ustring = (UChar*)lua_newuserdata((L), lua_objlen((L),-1));							\
			memcpy(___new_ustring, lua_tostring((L),-2), lua_objlen((L),-2));							\
			lua_insert((L),-2);																			\
			lua_pushvalue((L),-2);																		\
			lua_rawset((L),(pool_idx));																	\
			lua_pushvalue((L),(meta_idx));																\
			lua_setmetatable((L),-2);																	\
		}																								\
		else {																							\
			lua_remove((L),-2);																			\
		}																								\
	}

#define icu4lua_trustustring(L,i)		((UChar*)lua_touserdata((L),(i)))

#define icu4lua_pushrawustring(L,i)																		\
	lua_pushlstring(L, (const char*)lua_touserdata((L),(i)), lua_objlen((L),(i)))

#define icu4lua_addustring(B, ustr, ustr_len)															\
	luaL_addlstring((B), (const char*)(ustr), (ustr_len) * sizeof(UChar))

#define icu4lua_trustufile(L,i)			(*(UFILE**)lua_touserdata((L),(i)))

#define icu4lua_checkufile(L,i,meta_idx)																\
	(																									\
		luaL_argcheck(																					\
			(L),																						\
			(lua_getmetatable((L),(i)) && lua_rawequal((L),-1,(meta_idx)) && (lua_pop(L,1),1)),			\
			(i),																						\
			"expecting ufile"																			\
		),																								\
		icu4lua_trustufile((L),(i))																		\
	)

#define icu4lua_checkopenufile(L,i,meta_idx)															\
	(																									\
		luaL_argcheck(																					\
			(L),																						\
			lua_getmetatable((L),(i)) && lua_rawequal((L),-1,(meta_idx)) && (lua_pop(L,1),1),			\
			(i),																						\
			"expecting ufile"																			\
		),																								\
		luaL_argcheck(																					\
			(L),																						\
			icu4lua_trustufile((L),(i)) != NULL,														\
			(i),																						\
			"attempt to use a closed ufile"																\
		),																								\
		icu4lua_trustufile((L),(i))																		\
	)

#define icu4lua_trustregex(L,i)			(*(URegularExpression**)lua_touserdata((L),(i)))

#define icu4lua_checkregex(L,i,meta_idx)																\
	(																									\
		luaL_argcheck(																					\
			(L),																						\
			(lua_getmetatable((L),(i)) && lua_rawequal((L),-1,(meta_idx)) && (lua_pop(L,1),1)),			\
			(i),																						\
			"expecting regex"																			\
		),																								\
		icu4lua_trustregex((L),(i))																		\
	)

#define icu4lua_prepubuffer(b)			((UChar*)luaL_prepbuffer(b))
#define icu4lua_addusize(b,size)		luaL_addsize((b), (size)*sizeof(UChar))

#define icu4lua_pushuresult(b,meta_idx,pool_idx)														\
	{ luaL_pushresult(b); icu4lua_internrawustring((b)->L,meta_idx,pool_idx); }

#define ICU4LUA_UBUFFERSIZE				(LUAL_BUFFERSIZE/sizeof(UChar))
