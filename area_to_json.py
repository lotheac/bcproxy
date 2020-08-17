#!/usr/bin/env python3

import networkx
import psycopg2
import sys
import json
import random

# TODO: cardinal-position map drawing works very well. Figure out how to split
# into subgraphs that don't need explicit positioning, OR, draw as completely
# separate graphs (losing connecting edges maybe, but get some other method?)

# we ALSO need subgraphs for disconnected nodes (ie. nodes not found in the
# traversal; use them as new starting points for traversal until we have
# visited the entire forest)

G = networkx.MultiDiGraph()

conn = psycopg2.connect('dbname=batmud')
cur = conn.cursor()

cur.execute('select room.id, shortdesc, longdesc, indoors, exits from room '
            'where area=%s', (sys.argv[1],))
# as nodes, we use the room identifiers, but add json-serializable data
# formatted for sigma.js as node attrs
for row in cur:
    node = dict(zip(['id', 'label', 'longdesc', 'indoors', 'exits'], row))
    node['size'] = 1
    G.add_node(node['id'], **node)

cur.execute('select direction, source, destination from exit left join room '
            'on room.id=source where room.area=%s', (sys.argv[1],))
for row in cur:
    direction, src, tgt = row
    G.add_edge(src, tgt, key=direction)

posmod = {
    'north':        (0, -1),
    'south':        (0, 1),
    'west':         (-1, 0),
    'east':         (1, 0),
    'northwest':    (-1, -1),
    'northeast':    (1, -1),
    'southwest':    (-1, 1),
    'southeast':    (1, 1),
}

grid = {}
visited = set()

# networkx traversal algorithms only return (src, tgt) tuples for edges, but in
# a multidigraph that is not sufficient (we'd also need the key), so we
# implement traversal here

def place(node, x, y):
    G.node[node]['x'] = x
    G.node[node]['y'] = y
    if (x,y) in grid:
        print("COLLISION",file=sys.stderr)
    grid[x, y] = node

def visit(node):
    nattrs = G.node[node]
    # first place all the neighbors
    for nbr, edgekeys in G[node].items():
        if 'x' in G.node[nbr]:
            # nbr already placed
            continue
        # Because this is a MultiDiGraph, we can have more than one edge from
        # this node to the same neighbor. We find one edge keyed on a cardinal
        # direction and use that for placement, if any.
        for direction in edgekeys:
            if direction in posmod:
                dx, dy = posmod[direction]
                newpos = (nattrs['x'] + dx, nattrs['y'] + dy)
                place(nbr, *newpos)
                break
            else:
                print("new subarea...", file=sys.stderr)
                # XXX really not a good way to place. we can't really guess
                # whether there's going to be enough space for an entire
                # subgraph whereever we're placing it; maybe we should build
                # another one recursively and then find some space where it
                # fits?
                place(nbr, nattrs['x'] + 4, nattrs['y'] + 6)

    visited.add(node)
    # and then recurse into them if necessary
    for nbr in G[node]:
        if nbr not in visited:
            visit(nbr)

first = next(iter(G))
place(first, 0, 0)
visit(first)

# format the edge data for sigma
edges = []

for src, tgt, direction in G.edges:
    edge = {
        # sigma.js wants unique edge ids
        'id': '%s from %s' % (direction, src),
        'source': src,
        'target': tgt,
    }
    if not direction in posmod:
        edge['label'] = direction
        edge['type'] = 'curve'
        edge['color'] = '#9e9'
    edges.append(edge)

top = {'nodes': list(G.node.values()), 'edges': edges}
print(json.dumps(top, indent=2))
