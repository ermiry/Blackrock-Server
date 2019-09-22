FROM ermiry/ubuntu-cmongo:latest

RUN apt-get update && apt-get install -y libpthread-stubs0-dev sqlite3 libsqlite3-dev

WORKDIR /app/cerver
COPY . .
RUN make -j4

CMD ["make", "run"]