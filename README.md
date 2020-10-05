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
 - connection to BatMUD is encrypted with libtls
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
   game). When the room you are in changes, the room id and ingame area name
   are displayed. The room may become unknown due to a variety of reasons; when
   that happens, the `room_unknown` message is displayed. Examples:
```
∴room $apr1$dF!!_X#W$i8ByJsY5G/kpbE1RGJzqX1 mage guild
∴room_unknown hallucinating
```
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
 - cast status: prints lines when the game tells the client to update a
   skill/spell progress indicator. rounds left are sent by the game when
   skill/spell starts or is in progress, and 0 signifies unknown number of
   rounds. `cast_cancelled` means no skill or spell is in progress any more.
   Examples, with game-provided heartbeats "Dunk dunk" shown, below:
```
You start chanting.
Golden arrow: ####
∴cast golden_arrow 4
Dunk dunk
∴cast golden_arrow 0
Dunk dunk
You skillfully cast the spell with haste.
Golden arrow: #
∴cast golden_arrow 1
Dunk dunk
You are done with the chant.
spec_spell: Cast golden arrow at what?
∴cast_cancelled

You start concentrating on the skill.
∴use ceremony 0
∴use ceremony 0
Dunk dunk
You are prepared to do the skill.
spec_skill: You perform the ceremony.
∴cast_cancelled
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
