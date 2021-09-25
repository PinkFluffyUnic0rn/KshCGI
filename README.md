ksh-cgi

Tool that alows to use Korn shell as a server-side language in web development.

It parses CGI-requests from web-server and translates all input data into Korn Shell's
associative array "formdata", where keys are input's names and values are their values.

Currenly can run only in CGI mode (not FastCGI) and has been tested only with lighttpd.
