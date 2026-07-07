FROM ccr.ccs.tencentyun.com/library/ubuntu:20.04

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /user_fs

COPY . .

RUN make clean && make

CMD ["./user_fs"]