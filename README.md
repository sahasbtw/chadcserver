# ChadCServer

Genuine attempt to learn c by writing a minimal http server, mostly by looking
through Man pages, almost no stackoverflow & absolutely No AI.

Still in very early stages as the server can only serve an `index.html` file.

### TODO

- [X] Implement argument parsing to choose a custom path and a port to serve.
    - [ ] Make Argument Parsing better.
- [X] Parse requests and respond with client's `User-Agent`.
- [ ] Implement host based routing.
- [X] Serve content of current directory if directory is not given.
