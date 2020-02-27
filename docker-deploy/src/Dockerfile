FROM gcc:9

RUN mkdir /code
RUN mkdir /var/log/erss
WORKDIR /code
add . /code/

ENTRYPOINT ["./run.sh"]