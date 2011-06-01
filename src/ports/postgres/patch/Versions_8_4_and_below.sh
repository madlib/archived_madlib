#!/bin/sh

# Unfortunately, server/nodes/parsenodes.h from PG < 9.0 contains struct
# elements that are C++ keywords. Since this file changed frequently even within
# major builds we do a simple find & substitute. Essentially, we replace
# - "Oid typeid;" by "Oid typeOid;"
# - "TypeName *typename;" by "TypeName *typename;"
# - "List *typename;" by "List *typeName;"
# (These are the names used by the PG 9 header files.)

sed -e 's/\(Oid[[:space:]]\{1,\}\)typeid;/\1typeOid;/g' \
    -e 's/\(TypeName[[:space:]]\{1,\}\*[[:space:]]*\)typename/\1typeName/g' \
    -e 's/\(List[[:space:]]\{1,\}\*[[:space:]]*\)typename/\1typeName/g' \
    server/nodes/parsenodes.orig.h > \
    server/nodes/parsenodes.h

# Likewise, server/nodes/primnodes.h uses the "using" keyword. We replace
# - "List *using;" by "List *usingClause;"

sed -e 's/\(List[[:space:]]\{1,\}\*[[:space:]]*\)using/\1usingClause/g' \
    server/nodes/primnodes.orig.h > \
    server/nodes/primnodes.h

# And onther C++ incompatible file, server/utils/builtins.h. Replaing
# - "const char *namespace," by "const char *qualifier,"

sed 's/\(const[[:space:]]\{1,\}char[[:space:]]*\*[[:space:]]*\)namespace[[:space:]]*,/\1qualifier,/g' \
    server/utils/builtins.orig.h > \
    server/utils/builtins.h
