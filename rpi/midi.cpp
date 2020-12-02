#include "midi.h"


#include <alsa/asoundlib.h>
#include <iostream>
#include <poll.h>

namespace {

  class AlsaMidi {
    public:
      AlsaMidi() { };
      ~AlsaMidi() { end(); }

      void begin();
      void end();

      bool sendSynth(const MidiEvent& me)   { return send(me, synthPort); }
      bool sendControl(const MidiEvent& me) { return send(me, controlPort); }
      bool receive(MidiEvent&);
      bool poll(TimeInterval timeout);

    private:
      snd_seq_t *seq = nullptr;
      snd_midi_event_t *mev = nullptr;
      int npfds = 0;
      struct pollfd *pfds = nullptr;

      int controlPort;
      int synthPort;

      bool send(const MidiEvent&, int port);

      bool errCheck(int, const char* = nullptr);
      bool errFatal(int, const char* = nullptr);
  };



  void AlsaMidi::begin() {
    int serr;
    serr = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
    if (errFatal(serr, "open sequencer")) return;

    serr = snd_seq_set_client_name(seq, "bicycle");
    if (errFatal(serr, "name sequencer")) return;

    controlPort = snd_seq_create_simple_port(seq, "controllers",
      SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE
        | SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
      SND_SEQ_PORT_TYPE_APPLICATION);
    if (errFatal(controlPort, "create in port")) return;

    synthPort = snd_seq_create_simple_port(seq, "synths",
      SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
      SND_SEQ_PORT_TYPE_APPLICATION);
    if (errFatal(synthPort, "create out port")) return;

    npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
    pfds = new struct pollfd[npfds];
    npfds = snd_seq_poll_descriptors(seq, pfds, npfds, POLLIN);

    serr = snd_midi_event_new(3, &mev);
    if (errFatal(serr, "create event decoder")) return;

    snd_midi_event_no_status(mev, 1); // 1 = turn off running status
  }

  void AlsaMidi::end() {
    if (pfds) {
      delete pfds;
    }
    if (mev) {
      snd_midi_event_free(mev);
      mev = nullptr;
    }
    if (seq) {
      snd_seq_close(seq);
      seq = nullptr;
    }
  }


  bool AlsaMidi::send(const MidiEvent& m, int port) {
    snd_seq_event_t ev;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);

    snd_midi_event_reset_encode(mev);
    auto n = snd_midi_event_encode(mev, m.bytes, sizeof(m.bytes), &ev);
    if (errCheck(n, "midi encode")) return false;

    int serr;
    serr = snd_seq_event_output_direct(seq, &ev);
    if (errCheck(serr, "event output")) return false;

    serr = snd_seq_drain_output(seq);
    errCheck(serr, "drain output");
    // serr = snd_seq_sync_output_queue(seq);
    // errCheck(serr, "sync output");

    return true;
  }

  bool AlsaMidi::receive(MidiEvent& m) {
    snd_seq_event_t *ev;

    auto q = snd_seq_event_input(seq, &ev);
    if (q == -EAGAIN) return false;
    if (errCheck(q, "event input")) return false;

    snd_midi_event_reset_decode(mev);
    auto n = snd_midi_event_decode(mev, m.bytes, sizeof(m.bytes), ev);
    // snd_seq_seq_free_event(ev);    // no longer needed in modern ALSA
    if (n == -ENOENT) return false;   // some other port event (like SUBSCRIBE)
    if (n == -ENOMEM) return false;   // sysex messages, don't fit in 3 bytes
    if (errCheck(n, "midi decode")) return false;

    return true;
  }

  bool AlsaMidi::poll(TimeInterval timeout) {
    int t = (timeout == forever)
      ? -1
      : std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
    auto r = ::poll(pfds, npfds, t);

    if (false) {
      unsigned short revents;
      auto e = snd_seq_poll_descriptors_revents(seq, pfds, npfds, &revents);
      if (errCheck(e, "poll revents")) return false;
      return revents & POLLIN;
    }
    return r > 0;
  }

  bool AlsaMidi::errCheck(int serr, const char* op) {
    if (serr >= 0) return false;
    std::cerr << "ALSA Seq error " << std::dec << serr;
    if (op) std::cerr << " in " << op;
    std::cerr << std::endl;
    return true;
  }

  bool AlsaMidi::errFatal(int serr, const char* op) {
    bool r = errCheck(serr, op);
    if (r) end();
    return r;
  }

}




class Midi::Impl : public AlsaMidi { };

void Midi::begin() {
  end();
  impl = new Impl;
  if (impl) impl->begin();
}

void Midi::end() {
  if (impl) {
    delete impl;
    impl = nullptr;
  }
}

bool Midi::sendSynth(const MidiEvent& ev) {
  return impl ? impl->sendSynth(ev) : false;
}

bool Midi::sendControl(const MidiEvent& ev) {
  return impl ? impl->sendControl(ev) : false;
}

bool Midi::receive(MidiEvent& ev) {
  return impl ? impl->receive(ev) : false;
}

bool Midi::poll(TimeInterval timeout) {
  return impl ? impl->poll(timeout) : false;
}

