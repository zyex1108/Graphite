#ifndef NETWORK_MODEL_MAGIC_H
#define NETWORK_MODEL_MAGIC_H

#include "network.h"
#include "core.h"

class NetworkModelMagic : public NetworkModel
{
   private:
      UInt64 _bytesSent;

   public:
      NetworkModelMagic(Network *net) : NetworkModel(net), _bytesSent(0) { }
      ~NetworkModelMagic() { }

      void routePacket(const NetPacket &pkt,
                       std::vector<Hop> &nextHops)
      {
         Hop h;
         h.final_dest = pkt.receiver;
         h.next_dest = pkt.receiver;
         h.time = pkt.time + 1;
         nextHops.push_back(h);

         _bytesSent += getNetwork()->getModeledLength(pkt);
      }

      void processReceivedPacket(NetPacket& pkt) {}

      void outputSummary(std::ostream &out)
      {
         out << "    bytes sent: " << _bytesSent << std::endl;
      }

      void enable()
      {}

      void disable()
      {}
};

#endif /* NETWORK_MODEL_MAGIC_H */
