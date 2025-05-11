FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt install -y build-essential libreadline-dev && \
    apt clean

WORKDIR /app

COPY . .

RUN make

CMD ["./fsshell", "volume/SampleVolume", "10000000", "512"]
