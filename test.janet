(import build/lua :as "lua")

# create a new lua states

(def lua (lua/new))
(def lua2 (lua/new))
(pp [lua lua2])

# get / set global lua object

(def l-math (lua :math))
(pp l-math)
(set (lua :a) @[1 2 3])
(set (lua :b) @{:x 1 :y 2})
(pp (lua :a))

# create new lua-object

(def l-table (lua @[1 2 3 @[5 6 7]]))
(def l-string (lua "this is a lua-string"))
(def l-number (lua 100))
(pp l-table)
(pp l-string)
(pp l-number)

# indexing lua-object

(def pi (l-math :pi))
(pp ((lua :a) 1))
(pp ((lua :package) :path))

(set ((lua :a) 1) 100)
(pp ((lua :a) 1))

# indexing "non-indexable" lua object return janet object

(pp l-string)
(pp (l-string :value))
(pp (l-number :value))

(set (l-number :value) 200)
(set (l-string :value) "another lua string")

(pp (l-string :value))
(pp (l-number :value))


# calling lua-function-object

(pp ((l-math :cos) (/ pi 2)))
((lua :print) "hello janet!")

(def l-chunk ((lua :loadstring) "print('hello janet!')"))
(l-chunk nil)

# mutiple args and multiple returns as tuples

(pp (((lua :string) :match) ["hello janet 123" "(%w+)%s+(%w+)%s+(%d+)"]))


# ffi :-)

(def lua-ffi ((lua :require) :ffi))
((lua-ffi :cdef) "int printf(const char *fmt, ...);")

(pp ((lua-ffi :C) :printf))

# to call callable lua "non function" object use lua/call

(lua/call ((lua-ffi :C) :printf) ["Hello %s!\n" "world"])

# ffi cdata

(def buf ((lua-ffi :new) ["uint8_t[?]" 10]))
(for i 0 10 (set (buf i) (* 10 i)))
(pp buf)
(pp (buf 2))
