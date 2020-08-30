Map browser
===========

bcproxy stores room information in a postgresql database. This webapp
is used to display that data.

Setup
=====

- install python modules
  ```
  pip3 install -U Flask SqlAlchemy psycopg2 Flask-SqlAlchemy dataclasses dataclasses_json networkx
  ```
- run the web server
  ```
  flask run
  ```
- open your browser at http://127.0.0.1:5000/ (provided that there
  is room data in your databse)
