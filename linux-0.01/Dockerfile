FROM ubuntu:18.04

RUN apt-get update && apt-get install -y \
    bin86 gcc build-essential gcc-multilib \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /linux-0.01

COPY . ./

CMD ["bash"]
