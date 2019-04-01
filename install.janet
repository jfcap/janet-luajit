(def- is-win (= (os/which) :windows))
(def- is-mac (= (os/which) :macos))
(def- sep (if is-win "\\" "/"))
(def- objext (if is-win ".obj" ".o"))
(def- modext (if is-win ".dll" ".so"))

(defn- sys-install
       [& files]
       (let (cp (fn [src dest]
                  (print "copy " src " to " dest)
                  (spit dest (slurp src))))  
         (each f files
               (cp (string "build" sep f)
                   (string module/*syspath* sep f)))))

(sys-install (string "lua" modext))

