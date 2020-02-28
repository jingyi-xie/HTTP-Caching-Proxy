FROM gcc:9

RUN apt-get update && apt-get install -y netcat
RUN mkdir -p /code
RUN mkdir -p /var/log/erss
WORKDIR /code
add . /code/

ENTRYPOINT ["./run.sh"]