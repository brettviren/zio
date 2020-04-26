struct Impl;

struct Main
{
    Impl* imp;
    Main();
};

struct Impl
{
    Main& main;
    Impl(Main& m) : main(m) {}
};

Main::Main() : imp(new Impl(*this)) {}

int main() { Main m; }
