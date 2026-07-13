#!/usr/bin/env python3
# Doxygen INPUT_FILTER: a Q_PROPERTY(Type name READ name ...) line and its
# getter "Type name() const" would otherwise both parse as a Doxygen member
# named Class::name — one kind="variable", one kind="function" — which
# Breathe/Sphinx's C++ domain treats as the same symbol declared twice.
# Rather than emit anything for the Q_PROPERTY line, this drops it (and its
# doc comment) entirely and re-attaches that comment to the getter's own
# terse "@see name" line, which becomes the property's one and only
# surviving declaration. Only touches the doxygen-side parse; the real
# header on disk is untouched.
import re
import sys

PROPERTY_RE = re.compile( r"^\s*Q_PROPERTY\(\s*[A-Za-z_][A-Za-z0-9_]*\s+([A-Za-z_][A-Za-z0-9_]*)\s+READ\b" )
COMMENT_RE = re.compile( r"^\s*///" )
SEE_RE = re.compile( r"^\s*///\s*@see\s+([A-Za-z_][A-Za-z0-9_]*)\s*$" )


def main( path ):
	with open( path ) as f:
		lines = f.readlines()

	descriptions = {}
	drop = set()
	for i, line in enumerate( lines ):
		m = PROPERTY_RE.match( line )
		if not m:
			continue
		name = m.group( 1 )
		j = i - 1
		while j >= 0 and COMMENT_RE.match( lines[j] ):
			j -= 1
		comment_start = j + 1
		descriptions[name] = lines[comment_start:i]
		drop.update( range( comment_start, i + 1 ) )

	out = []
	for i, line in enumerate( lines ):
		if i in drop:
			continue
		m = SEE_RE.match( line )
		if m and m.group( 1 ) in descriptions:
			out.extend( descriptions[m.group( 1 )] )
			continue
		out.append( line )

	sys.stdout.writelines( out )


if __name__ == "__main__":
	main( sys.argv[1] )
