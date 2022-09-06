#include "server.h"

int main()
{
    server broker(1883);
    broker.start();
    broker.update();

    std::cout << "END\n";
    return 0;
}
