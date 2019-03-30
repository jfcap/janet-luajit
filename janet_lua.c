/*
* Copyright (c) 2019 J.-F. Cap & contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#include <janet/janet.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


typedef struct {
    lua_State *L;
} jl_vm;

typedef struct {
    jl_vm *vm;
    int ref;
    int type;
} jl_object;


void jl_vm_init(jl_vm *vm) {
    vm->L = lua_open();
    luaL_openlibs(vm->L);
}

void jl_vm_deinit(jl_vm *vm) {
    lua_close(vm->L);
}


static int jl_vm_gc(void *p, size_t s) {
    (void) s;
    jl_vm *vm = (jl_vm *)p;
    jl_vm_deinit(vm);
    return 0;
}

static int jl_object_gc(void *p, size_t s) {
    (void) s;
    jl_object *o = (jl_object *)p;
    luaL_unref(o->vm->L, LUA_REGISTRYINDEX, o->ref);
    return 0;
}

static int jl_object_mark(void *p, size_t s) {
    (void) s;
    jl_object *o = (jl_object *)p;
    janet_mark(janet_wrap_abstract(o->vm));
    return 0;
}

static Janet lua_to_janet(jl_vm *vm, int index);
static void jl_push_janet(jl_vm *vm, Janet x);


static void jl_object_tostring(void *p, JanetBuffer *buffer) {
    char str[64];
    jl_object *o = (jl_object *)p;
    sprintf(str, "<lua/object %s %x>", lua_typename(o->vm->L, o->type), o->ref);
    janet_buffer_push_cstring(buffer, str);
}


static Janet jl_vm_getter(void *p, Janet key);
static void jl_vm_setter(void *p, Janet key, Janet value);

static const JanetAbstractType jl_vm_type = {
    "lua/vm",
    jl_vm_gc,
    NULL,
    jl_vm_getter,
    jl_vm_setter,
    NULL,
    NULL,
    NULL,
};

static Janet jl_object_getter(void *p, Janet key);
static void jl_object_setter(void *p, Janet key, Janet value);

static const JanetAbstractType jl_object_type = {
    "lua/object",
    jl_object_gc,
    jl_object_mark,
    jl_object_getter,
    jl_object_setter,
    NULL,
    NULL,
    jl_object_tostring,
};

static Janet jl_object_make(jl_vm *vm) {
    jl_object *o = (jl_object *)janet_abstract(&jl_object_type, sizeof(jl_object));
    o->type = lua_type(vm->L, -1);
    o->ref = luaL_ref(vm->L, LUA_REGISTRYINDEX);
    o->vm = vm;
    return janet_wrap_abstract(o);
}


/* convert lua object on the stack at index to Janet object */
/* NUMBER,BOOLEAN and STRING object are translated to Janet object */
/* other types are encapsulated in jl_object AbstractType */

static Janet lua_to_janet(jl_vm *vm, int index) {
    size_t l;
    lua_State *L = vm->L;
    if (lua_isnil(L, index)) {
        return janet_wrap_nil();
    }
    int type = lua_type(L, index);
    switch (type) {
        case LUA_TBOOLEAN: {
            return janet_wrap_boolean(lua_toboolean(L, index));
            break;
        }
        case LUA_TNUMBER: {
            return janet_wrap_number(lua_tonumber(L, index));
            break;
        }
        case LUA_TSTRING: {
            const char *str_val = lua_tolstring(L, index, &l);
            return janet_wrap_string(janet_string((const uint8_t *)str_val, (int32_t)l));
            break;
        }
        default : {
            lua_pushvalue(L, index);
            return jl_object_make(vm);
        }
    }
    return janet_wrap_nil();
}

/* convert janet object to lua object and push it on the lua stack */

static void jl_push_janet(jl_vm *vm, Janet x) {
    lua_State *L = vm->L;
    JanetType type = janet_type(x);
    switch (type) {
        case JANET_NIL:
            lua_pushnil(L);
            return;
        case JANET_BOOLEAN:
            lua_pushboolean(L, janet_unwrap_boolean(x));
            return;
        case JANET_NUMBER:
            lua_pushnumber(L, janet_unwrap_number(x));
            return;
        case JANET_STRING:
        case JANET_SYMBOL:
        case JANET_KEYWORD: {
            const uint8_t *str = janet_unwrap_string(x);
            int32_t length = janet_string_length(str);
            lua_pushlstring(L, (const char *)str, (size_t)length);
            return;
        }
        case JANET_BUFFER: {
            JanetBuffer *buffer = janet_unwrap_buffer(x);
            lua_pushlstring(L, (const char *)(buffer->data), (size_t)(buffer->count));
            return;
        }
        case JANET_ARRAY: {
            int32_t i;
            JanetArray *a = janet_unwrap_array(x);
            lua_createtable(L, a->count, 0);
            for (i = 0; i < a->count; i++) {
                jl_push_janet(vm, a->data[i]);
                lua_rawseti(L, -2, i + 1);
            }
            return;
        }
        case JANET_TUPLE: {
            int32_t i, count;
            const Janet *tup = janet_unwrap_tuple(x);
            count = janet_tuple_length(tup);
            lua_createtable(L, count, 0);
            for (i = 0; i < count; i++) {
                jl_push_janet(vm, tup[i]);
                lua_rawseti(L, -2, i + 1);
            }
            return;
        }
        case JANET_TABLE: {
            int32_t i;
            JanetTable *t = janet_unwrap_table(x);
            lua_newtable(L);
            for (i = 0; i < t->capacity; i++) {
                if (janet_checktype(t->data[i].key, JANET_NIL))
                    continue;
                jl_push_janet(vm, t->data[i].key);
                jl_push_janet(vm, t->data[i].value);
                lua_rawset(L, -3);
            }
            return;
        }
        case JANET_STRUCT: {
            int32_t i;
            const JanetKV *struct_ = janet_unwrap_struct(x);
            lua_newtable(L);
            for (i = 0; i < janet_struct_capacity(struct_); i++) {
                if (janet_checktype(struct_[i].key, JANET_NIL))
                    continue;
                jl_push_janet(vm, struct_[i].key);
                jl_push_janet(vm, struct_[i].value);
                lua_rawset(L, -3);
            }
            return;
        }
        case JANET_ABSTRACT : {
            void *p = janet_unwrap_abstract(x);
            if (janet_abstract_type(p) == &jl_object_type) {
                jl_object *o = (jl_object *)p;
                lua_rawgeti(L, LUA_REGISTRYINDEX, o->ref);
                return;
            } else {
                janet_panicf("abstract value %p not convertible to lua object", x);
                return;
            }
        }
        default:
            janet_panicf("value %p not convertible to lua object", x);
            break;
    }

}

static Janet jl_vm_new(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 0);
    (void) argv;
    jl_vm *vm = (jl_vm *)janet_abstract(&jl_vm_type, sizeof(jl_vm));
    jl_vm_init(vm);
    return janet_wrap_abstract(vm);
}

static Janet jl_vm_getter(void *p, Janet key) {
    jl_vm *vm = (jl_vm *)p ;
    if (janet_checktype(key, JANET_KEYWORD)) {
        const uint8_t *name = janet_unwrap_keyword(key);
        lua_getglobal(vm->L, (const char *)name);
        return lua_to_janet(vm, -1);
    } else {
        jl_push_janet(vm, key);
        return jl_object_make(vm);
    }
    return janet_wrap_nil();
}

static void jl_vm_setter(void *p, Janet key, Janet value) {
    jl_vm *vm = (jl_vm *)p ;
    if (janet_checktype(key, JANET_KEYWORD)) {
        const uint8_t *name = janet_unwrap_keyword(key);
        jl_push_janet(vm, value);
        lua_setglobal(vm->L, (const char *)name);
    }
}

static Janet jl_object_call(jl_object *o, Janet args) {
    jl_vm *vm = o->vm;
    lua_State *L = vm->L;
    lua_settop(L, 0);
    Janet res = janet_wrap_nil();
    lua_rawgeti(L, LUA_REGISTRYINDEX, o->ref);
    int count = 1;
    if (janet_checktype(args, JANET_TUPLE)) { /* multiple args */
        int32_t i;
        const Janet *tup = janet_unwrap_tuple(args);
        count = janet_tuple_length(tup);
        for (i = 0; i < count; i++) {
            jl_push_janet(vm, tup[i]);
        }
    } else { /* one arg */
        jl_push_janet(vm, args);
    }
    int error = lua_pcall(L, count, LUA_MULTRET, 0);
    if (error) {
        janet_panic(lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
    }
    int nres = lua_gettop(L);
    if (nres > 1) {
        Janet *atuple = janet_tuple_begin(nres);
        for (int i = 0; i < nres; i++) {
            atuple[i] = lua_to_janet(vm, i + 1);
        }
        lua_settop(L, 0);
        res = janet_wrap_tuple(janet_tuple_end(atuple));
    } else if (nres == 1) {
        res = lua_to_janet(vm, 1);
        lua_pop(L, 1);
    }
    return res;
}


static Janet jl_object_getter(void *p, Janet key) {
    jl_object *o = (jl_object *)p ;
    lua_State *L = o->vm->L;
    Janet res = janet_wrap_nil();
    switch (o->type) {
        case LUA_TFUNCTION : {
            return jl_object_call(o, key);
        }
        case LUA_TNUMBER :
        case LUA_TSTRING :
        case LUA_TBOOLEAN : {
            lua_rawgeti(L, LUA_REGISTRYINDEX, o->ref);
            res = lua_to_janet(o->vm, -1);
            lua_pop(L, -1);
            return res;
        }
        default : {
            lua_rawgeti(L, LUA_REGISTRYINDEX, o->ref);
            jl_push_janet(o->vm, key);
            lua_gettable(L, -2);
            res = lua_to_janet(o->vm, -1);
            lua_pop(L, -1);
            return res;
        }
    }
    return res;
}


static void jl_object_setter(void *p, Janet key, Janet value) {
    jl_object *o = (jl_object *)p ;
    lua_State *L = o->vm->L;
    switch (o->type) {
        case LUA_TNUMBER :
        case LUA_TSTRING :
        case LUA_TBOOLEAN : {
            luaL_unref(L, LUA_REGISTRYINDEX, o->ref);
            jl_push_janet(o->vm, value);
            o->type = lua_type(L, -1);
            o->ref = luaL_ref(L, LUA_REGISTRYINDEX);
            break;
        }
        default : {
            lua_rawgeti(L, LUA_REGISTRYINDEX, o->ref);
            jl_push_janet(o->vm, key);
            jl_push_janet(o->vm, value);
            lua_settable(L, -3);
            lua_pop(L, 1);
            break;
        }
    }
}



static Janet jl_vm_call(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 2);
    jl_object *o = (jl_object *)janet_getabstract(argv, 0, &jl_object_type);
    return jl_object_call(o, argv[1]);
}





static const JanetReg cfuns[] = {
    {
        "lua/new", jl_vm_new,
        "(lua/new)\n\n"
        "Create new lua vm"
    },
    {
        "lua/call", jl_vm_call,
        "(lua/call lua-callee args)\n\n"
        "Call lua-callee object"
    },
    {NULL, NULL, NULL}
};




JANET_MODULE_ENTRY(JanetTable *env) {
    janet_cfuns(env, "lua", cfuns);
}
