
#include <luaconf.h>

#define CAP_SUCCESSFUL	(0)
#define CAP_UNFINISHED  (-1)
#define CAP_POSITION    (-2)

#define L_ESC           '%'

struct UMatchState;
typedef struct UMatchState UMatchState;

struct GmatchState;
typedef struct GmatchState GmatchState;

typedef void ProcessUCharIteratorRangeFunc(UMatchState* ms, uint32_t start_state, uint32_t end_state);
typedef void ProcessUMatchStateFunc(UMatchState* ms);

struct UMatchState {
	int level; // total number of captures, finished or unfinished
	lua_State *L;
	luaL_Buffer* b;
	void* context;
	ProcessUCharIteratorRangeFunc* pushRange;
	ProcessUCharIteratorRangeFunc* addRange;
	uint32_t end_state;
	struct {
		uint32_t start_state;
		uint32_t end_state;
		int what;
	} capture[LUA_MAXCAPTURES];
};

struct GmatchState {
	UMatchState ms;
	UCharIterator sourceIter;
	UCharIterator pattIter;
	uint32_t source_state;
	int matched_empty_end;
};

int iter_match(UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter, int init, int find);
int uiter_gsub_aux(UMatchState* ms, UCharIterator* pPattIter, UCharIterator* pSourceIter, ProcessUMatchStateFunc on_match, int max_s);
int gmatch_aux(lua_State *L);

#define uchar(c)        ((unsigned char)(c))
