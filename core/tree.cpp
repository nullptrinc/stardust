/* SPDX-License-Identifier: MIT */
#include <algorithm>
#include <cmath>
#include <utility>
#include "tree.h"

namespace stardust {

namespace {

/* Focus moves run on their own clocks, whatever duration the app set. */
constexpr float focus_ms = 500, tilt_ms = 650, scroll_ms = 420;

float ease_out(double t) {
	return 1 - std::pow(1 - t, 3);
}

/* An underdamped spring: starts at once, overshoots by about 7% near
 * t = 0.3 and has come to rest by t = 1. */
float spring(double t) {
	return 1 - std::exp(-8 * t) * std::cos(8 * t);
}

float lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

Color lerp(const Color &a, const Color &b, float t) {
	return {lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), lerp(a.a, b.a, t)};
}

template <typename T>
void set_animated(Node &node, Animated<T> &property, const T &target, double now) {
	if (node.fresh || !node.animated || node.duration_ms <= 0) {
		property.value = property.from = property.to = target;
		property.running = false;
		return;
	}
	property.from = property.value; // retarget smoothly from wherever it is now
	property.to = target;
	property.start = now;
	property.running = true;
}

template <typename T>
bool advance(Animated<T> &property, double now, float duration_ms) {
	if (!property.running) {
		return false;
	}
	double t = std::min(1.0, (now - property.start) * 1000 / duration_ms);
	property.value = lerp(property.from, property.to, property.spring ? spring(t) : ease_out(t));
	if (t >= 1) {
		property.value = property.to;
		property.running = false;
	}
	return property.running;
}

float as_float(const Value &value) {
	if (auto *f = std::get_if<float>(&value)) {
		return *f;
	}
	if (auto *i = std::get_if<int>(&value)) {
		return float(*i);
	}
	return 0;
}

void set_color(Node &node, Animated<Color> &property, const Value &value, double now) {
	if (auto *color = std::get_if<Color>(&value)) {
		set_animated(node, property, *color, now);
	}
}

int as_int(const Value &value) {
	if (auto *i = std::get_if<int>(&value)) {
		return *i;
	}
	return int(as_float(value));
}

/* Jumps to from and springs back to 0, as a lean that settles. */
void kick(Node &node, Animated<float> &property, float from, double now) {
	if (node.fresh || !node.animated || from == 0) {
		property.value = property.from = property.to = 0;
		property.running = false;
		return;
	}
	property.value = property.from = from;
	property.to = 0;
	property.start = now;
	property.running = true;
}

} // namespace

Tree::Tree() = default;

Node *Tree::get(NodeId id) {
	auto found = nodes_.find(id);
	return found == nodes_.end() ? nullptr : &found->second;
}

void Tree::apply(const Mutation &mutation, double now) {
	switch (mutation.op) {
	case Mutation::Create: {
		Node node;
		node.id = mutation.node;
		node.kind = mutation.kind;
		node.opacity.value = node.opacity.to = 1;
		node.scale.value = node.scale.to = 1;
		node.focus.spring = node.tilt_x.spring = node.tilt_y.spring = true;
		nodes_[node.id] = std::move(node);
		return;
	}
	case Mutation::Destroy: {
		Node *node = get(mutation.node);
		if (!node) {
			return;
		}
		std::vector<NodeId> children = node->children;
		if (Node *parent = get(node->parent)) {
			auto &list = parent->children;
			list.erase(std::remove(list.begin(), list.end(), node->id), list.end());
		}
		if (root_ == mutation.node) {
			root_ = 0;
		}
		nodes_.erase(mutation.node);
		for (NodeId child : children) {
			apply({Mutation::Destroy, child}, now);
		}
		return;
	}
	case Mutation::Append: {
		Node *node = get(mutation.node);
		if (!node) {
			return;
		}
		if (mutation.parent == 0) {
			root_ = node->id;
			return;
		}
		if (Node *parent = get(mutation.parent)) {
			node->parent = parent->id;
			parent->children.push_back(node->id);
		}
		return;
	}
	case Mutation::Set:
		break;
	}

	Node *node = get(mutation.node);
	if (!node) {
		return;
	}
	const Value &value = mutation.value;
	switch (mutation.prop) {
	case Prop::Background:
		if (auto *color = std::get_if<Color>(&value)) {
			set_animated(*node, node->background, *color, now);
		}
		break;
	case Prop::Radius: node->radius = as_float(value); break;
	case Prop::Opacity: set_animated(*node, node->opacity, as_float(value), now); break;
	case Prop::Scale: set_animated(*node, node->scale, as_float(value), now); break;
	case Prop::TranslationX: set_animated(*node, node->tx, as_float(value), now); break;
	case Prop::TranslationY: set_animated(*node, node->ty, as_float(value), now); break;
	case Prop::Text:
		if (auto *text = std::get_if<std::string>(&value)) {
			node->text = *text;
		}
		break;
	case Prop::TextColor:
		if (auto *color = std::get_if<Color>(&value)) {
			node->text_color = *color;
		}
		break;
	case Prop::Points:
		if (auto *points = std::get_if<std::vector<float>>(&value)) node->points = *points;
		break;
	case Prop::FontSize: node->font_size = as_float(value); break;
	case Prop::Bold: node->bold = as_int(value) != 0; break;
	case Prop::Layout: node->layout = Layout(as_int(value)); break;
	case Prop::Orientation: node->orientation = Orientation(as_int(value)); break;
	case Prop::Spacing: node->spacing = as_float(value); break;
	case Prop::Padding: node->padding = as_float(value); break;
	case Prop::PreferredWidth: node->preferred_width = as_float(value); break;
	case Prop::PreferredHeight: node->preferred_height = as_float(value); break;
	case Prop::HorizontalAlign: node->horizontal = Align(as_int(value)); break;
	case Prop::VerticalAlign: node->vertical = Align(as_int(value)); break;
	case Prop::Animated: node->animated = as_int(value) != 0; break;
	case Prop::Duration: node->duration_ms = as_float(value); break;
	case Prop::BackgroundEnd: set_color(*node, node->background_end, value, now); break;
	case Prop::BorderColor: set_color(*node, node->border_color, value, now); break;
	case Prop::ShadowColor: set_color(*node, node->shadow_color, value, now); break;
	case Prop::GlowColor: set_color(*node, node->glow_color, value, now); break;
	case Prop::Gloss: set_animated(*node, node->gloss, as_float(value), now); break;
	case Prop::Bevel: set_animated(*node, node->bevel, as_float(value), now); break;
	case Prop::InnerShadow: set_animated(*node, node->inner_shadow, as_float(value), now); break;
	case Prop::Shadow: set_animated(*node, node->shadow, as_float(value), now); break;
	case Prop::Glow: set_animated(*node, node->glow, as_float(value), now); break;
	case Prop::InnerGlow: set_animated(*node, node->inner_glow, as_float(value), now); break;
	case Prop::LightAngle: set_animated(*node, node->light, as_float(value), now); break;
	case Prop::BorderWidth: node->border_width = as_float(value); break;
	case Prop::Material: node->surface = Surface(as_int(value)); break;
	case Prop::TextShadowColor:
		if (auto *color = std::get_if<Color>(&value)) node->text_shadow = *color;
		break;
	case Prop::TextShadowOffset: node->text_shadow_offset = as_float(value); break;
	case Prop::TextGlow: node->text_glow = as_float(value); break;
	case Prop::TextGlowColor:
		if (auto *color = std::get_if<Color>(&value)) node->text_glow_color = *color;
		break;
	case Prop::Focusable: node->focusable = as_int(value) != 0; break;
	case Prop::FocusScale: node->focus_scale = as_float(value); break;
	case Prop::FocusShadow: node->focus_shadow = as_float(value); break;
	case Prop::FocusHighlight: node->focus_highlight = as_float(value); break;
	case Prop::FocusTilt: node->focus_tilt = as_float(value); break;
	case Prop::Focus: requested_ = node->id; break;
	case Prop::Scroll:
		node->scroll = ScrollAxis(as_int(value));
		if (node->scroll != ScrollAxis::Horizontal) set_animated(*node, node->scroll_x, 0.0f, now);
		if (node->scroll != ScrollAxis::Vertical) set_animated(*node, node->scroll_y, 0.0f, now);
		break;
	}
}

bool Tree::step(double now) {
	bool running = false;
	for (auto &[id, node] : nodes_) {
		running |= advance(node.background, now, node.duration_ms);
		running |= advance(node.opacity, now, node.duration_ms);
		running |= advance(node.scale, now, node.duration_ms);
		running |= advance(node.tx, now, node.duration_ms);
		running |= advance(node.ty, now, node.duration_ms);
		for (Animated<Color> *color : {&node.background_end, &node.border_color,
				&node.shadow_color, &node.glow_color}) {
			running |= advance(*color, now, node.duration_ms);
		}
		for (Animated<float> *value : {&node.gloss, &node.bevel, &node.inner_shadow,
				&node.shadow, &node.glow, &node.inner_glow, &node.light}) {
			running |= advance(*value, now, node.duration_ms);
		}
		running |= advance(node.focus, now, focus_ms);
		running |= advance(node.tilt_x, now, tilt_ms);
		running |= advance(node.tilt_y, now, tilt_ms);
		running |= advance(node.scroll_x, now, scroll_ms);
		running |= advance(node.scroll_y, now, scroll_ms);
	}
	return running;
}

void Tree::settle() {
	for (auto &[id, node] : nodes_) {
		node.fresh = false;
	}
}

/* ---- layout: StackLayout and DockLayout, as in Cascades ---- */

void Tree::measure(const Node &node, TextMetrics &metrics, float &width, float &height) {
	width = height = 0;
	if (node.kind == Kind::Label) {
		width = metrics.text_width(node.text, node.font_size, node.bold);
		height = metrics.line_height(node.font_size);
	} else {
		bool vertical = node.orientation == Orientation::TopToBottom;
		size_t count = 0;
		for (NodeId id : node.children) {
			auto found = nodes_.find(id);
			if (found == nodes_.end()) {
				continue;
			}
			float w, h;
			measure(found->second, metrics, w, h);
			if (node.layout == Layout::Dock) {
				width = std::max(width, w);
				height = std::max(height, h);
			} else if (vertical) {
				width = std::max(width, w);
				height += h;
			} else {
				width += w;
				height = std::max(height, h);
			}
			count++;
		}
		if (node.layout == Layout::Stack && count > 1) {
			(vertical ? height : width) += node.spacing * (count - 1);
		}
		width += 2 * node.padding;
		height += 2 * node.padding;
	}
	if (node.preferred_width >= 0) {
		width = node.preferred_width;
	}
	if (node.preferred_height >= 0) {
		height = node.preferred_height;
	}
}

static float place(Align align, float start, float available, float size) {
	switch (align) {
	case Align::Center: return start + (available - size) / 2;
	case Align::End: return start + available - size;
	default: return start;
	}
}

void Tree::arrange(Node &node, float x, float y, float width, float height,
		TextMetrics &metrics) {
	node.x = x;
	node.y = y;
	node.width = width;
	node.height = height;
	float inner_x = x + node.padding, inner_y = y + node.padding;
	float inner_w = std::max(0.0f, width - 2 * node.padding);
	float inner_h = std::max(0.0f, height - 2 * node.padding);
	float cursor = node.orientation == Orientation::TopToBottom ? inner_y : inner_x;

	for (NodeId id : node.children) {
		auto found = nodes_.find(id);
		if (found == nodes_.end()) {
			continue;
		}
		Node &child = found->second;
		float w, h;
		measure(child, metrics, w, h);
		if (child.horizontal == Align::Fill) {
			w = node.layout == Layout::Stack && node.orientation == Orientation::LeftToRight ?
				w : inner_w;
		}
		if (child.vertical == Align::Fill) {
			h = node.layout == Layout::Stack && node.orientation == Orientation::TopToBottom ?
				h : inner_h;
		}
		if (node.layout == Layout::Dock) {
			arrange(child, place(child.horizontal, inner_x, inner_w, w),
				place(child.vertical, inner_y, inner_h, h), w, h, metrics);
		} else if (node.orientation == Orientation::TopToBottom) {
			arrange(child, place(child.horizontal, inner_x, inner_w, w), cursor, w, h, metrics);
			cursor += h + node.spacing;
		} else {
			arrange(child, cursor, place(child.vertical, inner_y, inner_h, h), w, h, metrics);
			cursor += w + node.spacing;
		}
	}
}

void Tree::layout(float width, float height, TextMetrics &metrics) {
	if (Node *root = get(root_)) {
		arrange(*root, 0, 0, width, height, metrics);
	}
}

/* ---- touch: deepest node under the point, honouring translations ---- */

NodeId Tree::hit(NodeId id, float x, float y, float ox, float oy) const {
	auto found = nodes_.find(id);
	if (found == nodes_.end()) {
		return 0;
	}
	const Node &node = found->second;
	ox += node.tx.value;
	oy += node.ty.value;
	float left = node.x + ox, top = node.y + oy;
	if (x < left || y < top || x >= left + node.width || y >= top + node.height ||
			node.opacity.value <= 0.01f) {
		return 0;
	}
	ox -= node.scroll_x.value;
	oy -= node.scroll_y.value;
	for (auto child = node.children.rbegin(); child != node.children.rend(); ++child) {
		if (NodeId found_child = hit(*child, x, y, ox, oy)) {
			return found_child;
		}
	}
	return node.id;
}

NodeId Tree::hit(float x, float y) const {
	return hit(root_, x, y, 0, 0);
}

/* ---- focus: spatial navigation for remote controls ---- */

bool Tree::target_rect(NodeId id, Rect &rect) const {
	auto found = nodes_.find(id);
	if (found == nodes_.end()) {
		return false;
	}
	const Node &node = found->second;
	rect = {node.x + node.tx.to, node.y + node.ty.to, node.width, node.height};
	bool visible = node.opacity.to > 0.01f && node.width > 0 && node.height > 0;
	NodeId top = id;
	for (NodeId parent = node.parent; parent; ) {
		auto ancestor = nodes_.find(parent);
		if (ancestor == nodes_.end()) {
			return false;
		}
		const Node &a = ancestor->second;
		rect.x += a.tx.to - a.scroll_x.to;
		rect.y += a.ty.to - a.scroll_y.to;
		visible &= a.opacity.to > 0.01f;
		top = parent;
		parent = a.parent;
	}
	return visible && top == root_; // detached subtrees are never shown
}

void Tree::focus(NodeId id, float dx, float dy, double now) {
	if (id == focused_) {
		return;
	}
	if (Node *old = get(focused_)) {
		set_animated(*old, old->focus, 0.0f, now);
	}
	focused_ = id;
	Node *node = get(id);
	if (!node) {
		return;
	}
	set_animated(*node, node->focus, 1.0f, now);
	kick(*node, node->tilt_x, dx, now);
	kick(*node, node->tilt_y, dy, now);
	reveal(id, now);
	target_rect(id, focus_rect_);
}

void Tree::reveal(NodeId id, double now) {
	const Node *node = get(id);
	for (NodeId parent = node ? node->parent : 0; parent; ) {
		Node *a = get(parent);
		if (!a) {
			return;
		}
		parent = a->parent;
		if (a->scroll == ScrollAxis::None) {
			continue;
		}
		Rect child, box;
		target_rect(id, child);
		target_rect(a->id, box);
		bool horizontal = a->scroll == ScrollAxis::Horizontal;
		Animated<float> &scroll = horizontal ? a->scroll_x : a->scroll_y;
		/* In content coordinates; the padding leaves room for the lift. */
		float start = (horizontal ? child.x - box.x : child.y - box.y) + scroll.to;
		float size = horizontal ? child.width : child.height;
		float view = horizontal ? box.width : box.height;
		float target = scroll.to;
		if (start + size - target > view - a->padding) {
			target = start + size - (view - a->padding);
		}
		if (start - target < a->padding) {
			target = start - a->padding;
		}
		float extent = 0;
		for (NodeId c : a->children) {
			if (const Node *n = get(c)) {
				extent = std::max(extent, horizontal ? n->x + n->width - a->x : n->y + n->height - a->y);
			}
		}
		extent += a->padding;
		target = std::clamp(target, 0.0f, std::max(0.0f, extent - view));
		if (target != scroll.to) {
			set_animated(*a, scroll, target, now);
		}
	}
}

NodeId Tree::neighbour(Direction direction) const {
	Rect from;
	if (!target_rect(focused_, from)) {
		return 0;
	}
	NodeId best = 0;
	bool best_beam = false;
	float best_score = 0;
	for (const auto &[id, node] : nodes_) {
		Rect r;
		if (!node.focusable || id == focused_ || !target_rect(id, r)) {
			continue;
		}
		/* major: the gap along the direction; offset: between the centres
		 * across it; beam: overlapping across it, as a row or column does. */
		float major, offset;
		bool beam;
		switch (direction) {
		case Direction::Right:
			if (r.center_x() <= from.center_x() + 0.5f || r.x < from.x + 0.5f) continue;
			major = r.x - from.right();
			break;
		case Direction::Left:
			if (r.center_x() >= from.center_x() - 0.5f || r.right() > from.right() - 0.5f) continue;
			major = from.x - r.right();
			break;
		case Direction::Down:
			if (r.center_y() <= from.center_y() + 0.5f || r.y < from.y + 0.5f) continue;
			major = r.y - from.bottom();
			break;
		case Direction::Up:
			if (r.center_y() >= from.center_y() - 0.5f || r.bottom() > from.bottom() - 0.5f) continue;
			major = from.y - r.bottom();
			break;
		}
		if (direction == Direction::Left || direction == Direction::Right) {
			offset = std::fabs(r.center_y() - from.center_y());
			beam = r.y < from.bottom() && r.bottom() > from.y;
			if (!beam) {
				continue; // sideways stays in its band: the end of a row is an edge
			}
		} else {
			offset = std::fabs(r.center_x() - from.center_x());
			beam = r.x < from.right() && r.right() > from.x;
		}
		major = std::max(0.0f, major);
		float score = 13 * major * major + offset * offset; // as Android's FocusFinder
		bool better = !best || (beam && !best_beam) ||
			(beam == best_beam && (score < best_score || (score == best_score && id < best)));
		if (better) {
			best = id;
			best_beam = beam;
			best_score = score;
		}
	}
	return best;
}

NodeId Tree::first_focusable(NodeId id) const {
	auto found = nodes_.find(id);
	if (found == nodes_.end()) {
		return 0;
	}
	Rect rect;
	if (found->second.focusable && target_rect(id, rect)) {
		return id;
	}
	for (NodeId child : found->second.children) {
		if (NodeId first = first_focusable(child)) {
			return first;
		}
	}
	return 0;
}

NodeId Tree::nearest_focusable(float x, float y) const {
	NodeId best = 0;
	float best_distance = 0;
	for (const auto &[id, node] : nodes_) {
		Rect r;
		if (!node.focusable || !target_rect(id, r)) {
			continue;
		}
		float distance = std::hypot(r.center_x() - x, r.center_y() - y);
		if (!best || distance < best_distance || (distance == best_distance && id < best)) {
			best = id;
			best_distance = distance;
		}
	}
	return best;
}

void Tree::settle_focus(double now) {
	if (NodeId requested = std::exchange(requested_, 0)) {
		const Node *node = get(requested);
		Rect rect;
		if (node && node->focusable && target_rect(requested, rect)) {
			focus(requested, 0, 0, now);
		}
	}
	Rect rect;
	const Node *node = get(focused_);
	if (focused_ && (!node || !node->focusable || !target_rect(focused_, rect))) {
		if (!node) {
			focused_ = 0; // gone: nothing to let down
		}
		focus(nearest_focusable(focus_rect_.center_x(), focus_rect_.center_y()), 0, 0, now);
	} else if (focused_) {
		focus_rect_ = rect;
	} else if (NodeId first = first_focusable(root_)) {
		focus(first, 0, 0, now);
	}
}

float Tree::update_raised(NodeId id) {
	Node *node = get(id);
	if (!node) {
		return 0;
	}
	float raised = std::max(0.0f, node->focus.value);
	for (NodeId child : node->children) {
		raised = std::max(raised, update_raised(child));
	}
	node->raised = raised;
	return raised;
}

void Tree::update_raised() {
	update_raised(root_);
}

} // namespace stardust
