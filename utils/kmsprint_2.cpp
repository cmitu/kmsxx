#include <cinttypes>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>
#include <fmt/format.h>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;
using namespace kms;

static string format_mode(const Videomode& m, unsigned idx, unsigned encoder, const bool is_crtc = false)
{
	string str = fmt::format("Mode: {} {} {} {}",
				  idx,
				  is_crtc ? "crtc" : "connector",
				  encoder,
				  m.to_string_rp_custom());

	return str;
}

static string format_mode_short(const Videomode& m)
{
	return m.to_string_long();
}

static string format_connector(Connector& c)
{
	string str;

	str = fmt::format("Connector {} ({}) {}",
			  c.idx(), c.id(), c.fullname());

	switch (c.connector_status()) {
	case ConnectorStatus::Connected:
		str += " (connected)";
		break;
	case ConnectorStatus::Disconnected:
		str += " (disconnected)";
		break;
	default:
		str += " (unknown)";
		break;
	}

	return str;
}

static string format_encoder(Encoder& e)
{
	return fmt::format("Encoder {} ({}) {}",
			   e.idx(), e.id(), e.get_encoder_type());
}

static string format_crtc(Crtc& c)
{
	string str;

	str = fmt::format("Crtc {} ({})", c.idx(), c.id());

	if (c.mode_valid())
		str += " " + format_mode_short(c.mode());

	return str;
}

static string format_plane(Plane& p)
{
	string str;

	str = fmt::format("Plane {} ({})", p.idx(), p.id());

	if (p.fb_id())
		str += fmt::format(" fb-id: {}", p.fb_id());

	string crtcs = join<Crtc*>(p.get_possible_crtcs(), " ", [](Crtc* crtc) { return to_string(crtc->idx()); });

	str += fmt::format(" (crtcs: {})", crtcs);

	if (p.card().has_atomic()) {
		str += fmt::format(" {},{} {}x{} -> {},{} {}x{}",
				   (uint32_t)p.get_prop_value("SRC_X") >> 16,
				   (uint32_t)p.get_prop_value("SRC_Y") >> 16,
				   (uint32_t)p.get_prop_value("SRC_W") >> 16,
				   (uint32_t)p.get_prop_value("SRC_H") >> 16,
				   (uint32_t)p.get_prop_value("CRTC_X"),
				   (uint32_t)p.get_prop_value("CRTC_Y"),
				   (uint32_t)p.get_prop_value("CRTC_W"),
				   (uint32_t)p.get_prop_value("CRTC_H"));
	}

	string fmts = join<PixelFormat>(p.get_formats(), " ", [](PixelFormat fmt) { return PixelFormatToFourCC(fmt); });

	str += fmt::format(" ({})", fmts);

	return str;
}

static string format_fb(Framebuffer& fb)
{
	return fmt::format("FB {} {}x{}",
			   fb.id(), fb.width(), fb.height());
}

static string format_ob(DrmObject* ob)
{
	if (auto o = dynamic_cast<Connector*>(ob))
		return format_connector(*o);
	else if (auto o = dynamic_cast<Encoder*>(ob))
		return format_encoder(*o);
	else if (auto o = dynamic_cast<Crtc*>(ob))
		return format_crtc(*o);
	else if (auto o = dynamic_cast<Plane*>(ob))
		return format_plane(*o);
	else if (auto o = dynamic_cast<Framebuffer*>(ob))
		return format_fb(*o);
	else
		EXIT("Unkown DRM Object type\n");
}

template<class T>
vector<T> filter(const vector<T>& sequence, function<bool(T)> predicate)
{
	vector<T> result;

	for (auto it = sequence.begin(); it != sequence.end(); ++it)
		if (predicate(*it))
			result.push_back(*it);

	return result;
}

static void print_modes(Card& card)
{
	for (Connector* conn : card.get_connectors()) {
		if (!conn->connected())
			continue;

		fmt::print("{}\n", format_ob(conn));

		unsigned encoder_id = {0};
		for (Encoder* const e : conn->get_encoders()) {
			fmt::print(" {}\n", format_encoder(*e));
			encoder_id = e->id();

			// Dump a map of Encoders and their Crtc(s)
			if (e->get_crtc()->mode_valid())
				fmt::print("Encoder map: {:2d} to {:2d}\n", e->id(), e->get_crtc()->id());
		}

		// print the Crtc(s) video mode, so we know which video mode is currently active
		unsigned crtc_index = 0;
		for (Crtc* crtc : card.get_crtcs()) {
			if (crtc->mode_valid())
				fmt::print("{}\n", format_mode(crtc->mode(), crtc_index, crtc->id(), true));
			crtc_index++;
		}

		auto modes = conn->get_modes();
		for (unsigned i = 0; i < modes.size(); ++i)
			fmt::print("{}\n", format_mode(modes[i], i, encoder_id));
	}
}

static const char* usage_str =
	"Usage: kmsprint-rp [OPTIONS]\n\n"
	"Options:\n"
	"      --device=DEVICE     DEVICE is the path to DRM card to open\n";

static void usage()
{
	puts(usage_str);
}

int main(int argc, char** argv)
{
	string dev_path;

	OptionSet optionset = {
		Option("|device=", [&dev_path](string s) {
			dev_path = s;
		}),
		Option("h|help", []() {
			usage();
			exit(-1);
		}),
	};

	optionset.parse(argc, argv);

	if (optionset.params().size() > 0) {
		usage();
		exit(-1);
	}

	Card card(dev_path);

	print_modes(card);

	return 0;
}
