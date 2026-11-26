/* SPDX-License-Identifier: MIT */
/* The render core's copy of the node tree: layout and implicit animation. */
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "scene.h"

namespace stardust {

/* A property that eases to each new value instead of jumping. */
template <typename T>
struct Animated {
	T value{}, from{}, to{};
	double start = 0;
	bool running = false;
	bool spring = false; // a damped spring that overshoots a little, not an ease-out
};

enum class Direction : uint8_t { Left, Right, Up, Down };

struct Node {
	NodeId id = 0;
	Kind kind = Kind::Container;
	NodeId parent = 0;
	std::vector<NodeId> children;

	Animated<Color> background, background_end, border_color, shadow_color, glow_color;
	Animated<float> gloss, bevel, inner_shadow, shadow, glow, inner_glow, light;
	float border_width = 0;
	Surface surface = Surface::Plain;
	float radius = 0;
	Animated<float> opacity, scale, tx, ty;

	std::string text;
	std::vector<float> points; // independent normalized line segments (x1,y1,x2,y2)
	Color text_color = Color::rgb(0xffffffff);
	float font_size = 16;
	bool bold = false;
	Color text_shadow, text_glow_color;
	float text_shadow_offset = 1, text_glow = 0;

	Layout layout = Layout::Stack;
	Orientation orientation = Orientation::TopToBottom;
	float spacing = 0, padding = 0;
	float preferred_width = -1, preferred_height = -1;
	Align horizontal = Align::Fill, vertical = Align::Start;
	bool animated = true;
	float duration_ms = 250;
	bool fresh = true; // values set before its first frame apply at once

	/* Focus: focus runs 0..1 on a spring; tilt is where it came from
	 * (-1..1 per axis) and springs back to 0. */
	bool focusable = false;
	float focus_scale = 1.1f, focus_shadow = 24, focus_highlight = 0.5f, focus_tilt = 6;
	Animated<float> focus, tilt_x, tilt_y;
	float raised = 0; // the largest focus in this subtree; drawn above its siblings
	ScrollAxis scroll = ScrollAxis::None;
	Animated<float> scroll_x, scroll_y; // content offset of a scroll container

	float x = 0, y = 0, width = 0, height = 0; // layout result, logical px
};

/* A rectangle in window coordinates, logical px. */
struct Rect {
	float x = 0, y = 0, width = 0, height = 0;
	float right() const { return x + width; }
	float bottom() const { return y + height; }
	float center_x() const { return x + width / 2; }
	float center_y() const { return y + height / 2; }
};

class TextMetrics {
public:
	virtual ~TextMetrics() = default;
	virtual float text_width(const std::string &text, float size, bool bold) = 0;
	virtual float line_height(float size) = 0;
};

class Tree {
public:
	Tree();
	void apply(const Mutation &mutation, double now);
	/* Advances animations; true while any is still running. */
	bool step(double now);
	void layout(float width, float height, TextMetrics &metrics);
	/* Topmost node under a point, or 0. */
	NodeId hit(float x, float y) const;
	void settle(); // marks every node as drawn once

	Node *get(NodeId id);
	NodeId root() const { return root_; }

	/* ---- focus (call after layout) ---- */
	NodeId focused() const { return focused_; }
	/* Moves the focus; (dx, dy) is the direction of travel, for the tilt. */
	void focus(NodeId id, float dx, float dy, double now);
	/* The nearest focusable node in a direction, or 0. */
	NodeId neighbour(Direction direction) const;
	/* Puts the focus right after mutations and layout: on a node asked for
	 * with Prop::Focus, off nodes that went away or out of sight (to the
	 * nearest one), and on the first focusable node when there is none. */
	void settle_focus(double now);
	/* Updates Node::raised for drawing order. */
	void update_raised();

private:
	void measure(const Node &node, TextMetrics &metrics, float &width, float &height);
	void arrange(Node &node, float x, float y, float width, float height, TextMetrics &metrics);
	NodeId hit(NodeId id, float x, float y, float ox, float oy) const;
	/* Where a node will be once translations and scrolling settle, and
	 * whether it will be visible; animations in flight are ignored. */
	bool target_rect(NodeId id, Rect &rect) const;
	/* Scrolls the node's scroll-container ancestors to show it. */
	void reveal(NodeId id, double now);
	NodeId first_focusable(NodeId id) const;
	NodeId nearest_focusable(float x, float y) const;
	float update_raised(NodeId id);

	std::unordered_map<NodeId, Node> nodes_;
	NodeId root_ = 0;
	NodeId focused_ = 0, requested_ = 0;
	Rect focus_rect_; // where the focused node was, to refocus nearby when it goes
};

} // namespace stardust
