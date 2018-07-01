This is a TCP proxy intended to sit between your telnet MUD client and BatMUD.
It turns on "BatClient-mode" and filters the BatClient messages the server
sends.

Some extra lines are sent to the client by the proxy on reception of BatClient
messages. Such lines are prefixed with the marker character ∴ (U+2234
THEREFORE), encoded as `\xe2\x88\xb4` in UTF-8) to make it possible to match
them in your MUD client.

Features
========

 - more colors: 24-bit colors sent by BatMUD are approximated to closest-match
   xterm-256color escapes
 - tls support (always enabled if LibreSSL/libtls is available; cleartext
   telnet connections used otherwise)
 - convert utf-8 from client to iso-8859-1 when sending to server, and vice
   versa (utf-8 is always used; locales are not taken into account)
 - message classification: some server-sent messages have an additional type
   visible at the start of the line. these are *not* prefixed with the marker
   character, since the messages would be visible also to normal telnet clients
   (although without type). examples:
```
chan_tell: You tell Lotheac 'hello'
spec_spell: You watch with self-pride as your golden arrow hits Adult skunk.
```
 - mapping: room descriptions & exits (edges between rooms) are stored in a
   database as you play (requires `set client_mapper_toggle on` configured in
   game). no support yet for map visualization.
 - prots: a line with duration is displayed for every prot status update sent
   by BatMUD. you should set up client triggers for this. example:
```
∴prots force_absorption 805
∴prots displacement 380
∴prots force_absorption 802
∴prots displacement 377
```
 - target status: when BatMUD sends updates about your current target's health
   percentage, a line is printed. examples:
```
∴target Small_shrew 100
<omitted text>
Small shrew is DEAD, R.I.P.
∴target 0 0
```

Setup
=====

 - install postgreqsl client libraries (`libpq` or `libpq-dev`) and a
   postgresql server
 - run `./configure`
 - compile with BSD make (`bmake` on Linuxes): `make obj && make`
 - set up a postgresql database named `batmud`
 - initialize the db `psql batmud < initdb.sql`
 - `obj/bcproxy 1234`
 - connect your mud client to localhost:1234

Bugs
====

Stacking of control codes is not completely correct (ref. proxy.c:on\_open),
which means that setting both background and foreground colors doesn't work
currently.
