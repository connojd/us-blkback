FROM    alpine:latest
LABEL   authors="Jason Giroux <girouxj@ainfosec.com>"

RUN     apk --no-cache add --update bash sudo nano 'su-exec>=0.2'

# download and install depend:

RUN apk --no-cache add --update
RUN apk --no-cache add g++
RUN apk --no-cache add cmake
RUN apk --no-cache add make
RUN apk --no-cache add gcc
RUN apk --no-cache add xen-dev

RUN     rm -rf /var/cache/apk/*



