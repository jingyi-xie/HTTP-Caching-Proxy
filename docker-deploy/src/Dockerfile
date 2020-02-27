FROM ubuntu:latest
RUN apt-get -y update && apt-get install -y && \
    apt-get -y install c++17 g++9 && \
    apt-get -y install libstdc++6 && \
    apt-get -y install make
RUN mkdir /var/log/erss
ADD . /var/log/erss/
WORKDIR /var/log/erss
ENTRYPOINT ["./run.sh"]
