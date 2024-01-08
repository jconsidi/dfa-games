FROM ubuntu:22.04

# environment

ARG DEBIAN_FRONTEND=noninteractive

# baseline setup

RUN apt-get update

RUN apt-get update --fix-missing \
 && apt-get install -y \
  build-essential \
  g++ \
  libssl-dev \
  libtbb-dev \
  lldb \
  make \
  openssl \
  tzdata

# custom start

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8
ENV HOME /dfa-games
ENV TZ US/Eastern
WORKDIR $HOME

# real content here. changes frequently.

RUN mkdir $HOME/scratch
ADD src/setup-scratch.sh $HOME/
RUN cat ./setup-scratch.sh | perl -p -e 's/\r//' | /bin/bash

ADD src/Makefile $HOME/
ADD src/*.h $HOME/
ADD src/*.cpp $HOME/

RUN make
