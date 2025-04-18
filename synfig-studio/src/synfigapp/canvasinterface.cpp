/* === S Y N F I G ========================================================= */
/*!	\file canvasinterface.cpp
**	\brief Template File
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**	Copyright (c) 2009 Carlos A. Sosa Navarro
**  Copyright (c) 2011 Carlos López
**
**	This file is part of Synfig.
**
**	Synfig is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 2 of the License, or
**	(at your option) any later version.
**
**	Synfig is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with Synfig.  If not, see <https://www.gnu.org/licenses/>.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <synfig/general.h>

#include <synfig/canvasfilenaming.h>
#include <synfig/context.h>
#include <synfig/gradient.h>
#include <synfig/guidset.h>
#include <synfig/importer.h>
#include <synfig/loadcanvas.h>
#include <synfig/bone.h>
#include <synfig/pair.h>
#include <synfig/waypoint.h>
#include <synfig/valuenode_registry.h>
#include <synfig/surface.h>
#include <synfig/rendering/software/surfacesw.h>

#include <synfig/layers/layer_pastecanvas.h>
#include <synfig/layers/layer_bitmap.h>

#include <synfig/valuenodes/valuenode_animatedfile.h>
#include <synfig/valuenodes/valuenode_animatedfile.h>
#include <synfig/valuenodes/valuenode_bline.h>
#include <synfig/valuenodes/valuenode_linear.h>
#include <synfig/valuenodes/valuenode_composite.h>
#include <synfig/valuenodes/valuenode_dilist.h>
#include <synfig/valuenodes/valuenode_reference.h>
#include <synfig/valuenodes/valuenode_scale.h>
#include <synfig/valuenodes/valuenode_stripes.h>
#include <synfig/valuenodes/valuenode_subtract.h>
#include <synfig/valuenodes/valuenode_timedswap.h>
#include <synfig/valuenodes/valuenode_twotone.h>
#include <synfig/valuenodes/valuenode_wplist.h>

#include <synfigapp/localization.h>

#include "action_system.h"
#include "canvasinterface.h"
#include "instance.h"
#include "main.h"

#include "actions/editmodeset.h"
#include "actions/layeradd.h"
#include "actions/layerremove.h"
#include "actions/valuedescconvert.h"
#include "actions/valuenodeadd.h"

#endif

/* === U S I N G =========================================================== */

using namespace synfig;
using namespace synfigapp;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

CanvasInterface::CanvasInterface(etl::loose_handle<Instance> instance,Canvas::Handle canvas):
	instance_(instance),
	canvas_(canvas),
	cur_time_(canvas->rend_desc().get_frame_start()),
	mode_(MODE_NORMAL|MODE_ANIMATE_PAST|MODE_ANIMATE_FUTURE),
	state_("")
{
	set_selection_manager(get_instance()->get_selection_manager());
	set_ui_interface(get_instance()->get_ui_interface());
}

CanvasInterface::~CanvasInterface()
{
	DEBUG_LOG("SYNFIG_DEBUG_DESTRUCTORS",
		"CanvasInterface::~CanvasInterface(): Deleted");
}

void
CanvasInterface::set_time(synfig::Time x)
{
	if(get_canvas()->rend_desc().get_frame_rate())
	{
		float fps(get_canvas()->rend_desc().get_frame_rate());
		Time r(x.round(fps));
		//synfig::info("CanvasInterface::set_time(): %s rounded to %s\n",x.get_string(fps).c_str(),r.get_string(fps).c_str());
		x=r;
	}
	if(cur_time_.is_equal(x))
		return;
	get_canvas()->set_time(cur_time_=x);

	// update the time in all the child canvases
	Canvas::Children children = get_canvas()->get_root()->children();
	etl::handle<CanvasInterface> interface;
	for (Canvas::Children::iterator iter = children.begin(); iter != children.end(); ++iter)
		if ((interface = get_instance()->find_canvas_interface(*iter)) != this)
			interface->set_time(interface->get_canvas()->get_time());

	signal_time_changed()();
}

synfig::Time
CanvasInterface::get_time()const
{
	return cur_time_;
}

void
CanvasInterface::refresh_current_values()
{
	get_canvas()->set_time(cur_time_);
	signal_time_changed()();
	signal_dirty_preview()();
}

etl::handle<CanvasInterface>
CanvasInterface::create(etl::loose_handle<Instance> instance, Canvas::Handle canvas)
{
	etl::handle<CanvasInterface> intrfc;
	intrfc=new CanvasInterface(instance,canvas);
	instance->canvas_interface_list().push_front(intrfc);
	return intrfc;
}

void
CanvasInterface::set_mode(Mode x)
{
	Action::Handle 	action(Action::EditModeSet::create());

	assert(action);

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("edit_mode",x);

	if(!action->is_ready())
	{
		get_ui_interface()->error(_("Action Not Ready, unable to change mode"));
		assert(0);
		return;
	}

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Unable to change mode"));

//	mode_=x;
//	signal_mode_changed_(x);
}

CanvasInterface::Mode
CanvasInterface::get_mode()const
{
	return mode_;
}

void
CanvasInterface::set_state(synfig::String x) {
	state_=x;
}

synfig::String
CanvasInterface::get_state()const
{
	return state_;
}

synfig::Layer::Handle
CanvasInterface::layer_create(
	const synfig::String &id,
	const synfig::Canvas::Handle &canvas )
{
	Layer::Handle layer = Layer::create(id);
	assert(layer);
	if (!layer)
		return Layer::Handle();

	if (canvas!=get_canvas() && !canvas->is_inline())
	{
		synfig::error("Bad canvas passed to \"layer_create\"");
		return 0;
	}

	// automatically export the Index parameter of new Duplicate layers
	if (id == "duplicate")
		for (int i = 1; ; i++)
		{
			String valuenode_id = strprintf(_("Index %d"), i);
			try
			{
				canvas->find_value_node(valuenode_id, true);
			}
			catch (const Exception::IDNotFound& x)
			{
				add_value_node(layer->dynamic_param_list().find("index")->second, valuenode_id);
				break;
			}
		}

	layer->set_canvas(canvas);
	if (Layer_PasteCanvas::Handle::cast_dynamic(layer))
		layer->set_param("canvas", Canvas::create_inline(canvas));

	return layer;
}

void
CanvasInterface::layer_set_defaults(const synfig::Layer::Handle &layer)
{
	if (!layer || !layer->get_canvas())
		return;

	synfig::Canvas::Handle canvas = layer->get_canvas();
	synfig::String name(layer->get_name());

	ValueBase p;
	p=layer->get_param("fg");
	p.set(synfigapp::Main::get_outline_color());
	if(layer->set_param("fg",p))
	{
		p=layer->get_param("bg");
		p.set(synfigapp::Main::get_outline_color());
		layer->set_param("bg",p);
	}
	else if (name == "outline" || name == "advanced_outline")
	{
		p=layer->get_param("color");
		p.set(synfigapp::Main::get_outline_color());
		layer->set_param("color",p);
	}
	else
	{
		p=layer->get_param("color");
		p.set(synfigapp::Main::get_fill_color());
		layer->set_param("color",p);
	}
	// by default, new advanced outline layers are homogeneous
	if(name=="advanced_outline")
	{
		p=layer->get_param("homogeneous");
		p.set(true);
		layer->set_param("homogeneous",p);
	}
	p=layer->get_param("width");
	p.set(synfigapp::Main::get_bline_width().units(get_canvas()->rend_desc()));
	layer->set_param("width",p);
	
	p=layer->get_param("gradient");
	p.set(synfigapp::Main::get_gradient());
	layer->set_param("gradient",p);
	
	if(synfigapp::Main::get_blend_method() != Color::BLEND_BY_LAYER)
	{
		p=layer->get_param("blend_method");
		p.set((int)synfigapp::Main::get_blend_method());
		layer->set_param("blend_method",p);
	}


	{
		// Grab the layer's list of parameters
		Layer::ParamList paramlist=layer->get_param_list();
		Layer::ParamList::iterator iter;

		// loop through the static parameters
		for(iter=paramlist.begin();iter!=paramlist.end();++iter)
		{
			ValueNode::Handle value_node;

			// if we find any which are list values then make them
			// into dynamic list valuenodes, unless every element of
			// the list is a blinepoint, in which case convert it to a
			// bline
			if(iter->second.get_type()==type_list)
			{
				// check whether it's a list of blinepoints or widthpoints only
				std::vector<ValueBase> list(iter->second.get_list());
				if (list.size())
				{
					std::vector<ValueBase>::iterator iter2 = list.begin();
					Type &type(iter2->get_type());
					for (++iter2; iter2 != list.end(); ++iter2)
						if (iter2->get_type() != type)
							break;
					if (iter2 == list.end())
					{
						if (type == type_bline_point)
						{
							value_node=ValueNodeRegistry::create("bline",iter->second);
							ValueNode_BLine::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
						}
						else
						if (type == type_bone_object)
						{
							if (getenv("SYNFIG_USE_DYNAMIC_LIST_FOR_BONES"))
							{
								value_node=ValueNodeRegistry::create("dynamic_list",iter->second);
								ValueNode_DynamicList::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
							}
							else // this is the default
							{
								value_node=ValueNodeRegistry::create("static_list",iter->second);
								ValueNode_StaticList::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
							}
						}
						else
						if (type == types_namespace::TypePair<Bone, Bone>::instance)
						{
							if (getenv("SYNFIG_USE_DYNAMIC_LIST_FOR_BONES"))
							{
								value_node=ValueNodeRegistry::create("dynamic_list",iter->second);
								ValueNode_DynamicList::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
							}
							else // this is the default
							{
								value_node=ValueNodeRegistry::create("static_list",iter->second);
								ValueNode_StaticList::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
							}
						}
						else
						if (type == type_vector)
						{
							if (getenv("SYNFIG_USE_STATIC_LIST_FOR_VECTORS"))
							{
								value_node=ValueNodeRegistry::create("static_list",iter->second);
								ValueNode_StaticList::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
							}
							else // this is the default
							{
								value_node=ValueNodeRegistry::create("dynamic_list",iter->second);
								ValueNode_DynamicList::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
							}
						}
					}
					for (iter2 = list.begin(); iter2 != list.end(); ++iter2)
						if (iter2->get_type() != type_width_point)
							break;
					if (iter2 == list.end())
					{
						value_node=ValueNodeRegistry::create("wplist",iter->second);
						ValueNode_WPList::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
					}
					for (iter2 = list.begin(); iter2 != list.end(); ++iter2)
						if (iter2->get_type() != type_dash_item)
							break;
					if (iter2 == list.end())
					{
						value_node=ValueNodeRegistry::create("dilist",iter->second);
						ValueNode_DIList::Handle::cast_dynamic(value_node)->set_member_canvas(canvas);
					}
				}
				// it has something else so just insert the dynamic list
				if (!value_node)
					value_node=ValueNodeRegistry::create("dynamic_list",iter->second);
			}
			// otherwise, if it's a type that can be converted to
			// 'composite' (other than the types that can be radial
			// composite) then do so
			else if (ValueNodeRegistry::check_type("composite",iter->second.get_type()) &&
					 (iter->second.get_type()!=type_color &&
					  iter->second.get_type()!=type_vector))
				value_node=ValueNodeRegistry::create("composite",iter->second);

			if(value_node)
				layer->connect_dynamic_param(iter->first,value_node);
		}
	}
}

bool
CanvasInterface::layer_add_action(const synfig::Layer::Handle &layer)
{
	if (!layer || !layer->get_canvas())
		{ assert(false); return false; }

	Action::Handle action(Action::LayerAdd::create());
	if (!action)
		{ assert(false); return false; }

	action->set_param("canvas", layer->get_canvas());
	action->set_param("canvas_interface", etl::loose_handle<CanvasInterface>(this));
	action->set_param("new", layer);

	if(!action->is_ready())
		{ get_ui_interface()->error(_("Action Not Ready")); return false; }
	if(!get_instance()->perform_action(action))
		{ get_ui_interface()->error(_("Action Failed.")); return false; }

	return true;
}

bool
CanvasInterface::layer_move_action(const synfig::Layer::Handle &layer, int depth)
{
	if (!layer || !layer->get_canvas())
		{ assert(false); return false; }

	Action::Handle action(Action::create("LayerMove"));
	if (!action)
		{ assert(false); return false; }

	action->set_param("canvas", layer->get_canvas());
	action->set_param("canvas_interface", etl::loose_handle<CanvasInterface>(this));
	action->set_param("layer", layer);
	action->set_param("new_index", depth);

	if (!action->is_ready())
		{ get_ui_interface()->error(_("Move Action Not Ready")); return false; }
	if (!get_instance()->perform_action(action))
		{ get_ui_interface()->error(_("Move Action Failed.")); return false; }

	return true;
}

Layer::Handle
CanvasInterface::add_layer_to(const synfig::String &id, const synfig::Canvas::Handle &canvas, int depth)
{
	synfigapp::Action::PassiveGrouper group(get_instance().get(),_("Add Layer To"));

	Layer::Handle layer = layer_create(id, canvas);
	if (!layer) return Layer::Handle();

	layer_set_defaults(layer);
	layer_add_action(layer);
	if (depth != 0)
		layer_move_action(layer, depth);

	return layer;
}

bool
CanvasInterface::convert(ValueDesc value_desc, synfig::String type)
{
	Action::Handle 	action(Action::ValueDescConvert::create());

	assert(action);
	if(!action)
		return 0;

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("value_desc",value_desc);
	action->set_param("type",type);
	action->set_param("time",get_time());

	if(!action->is_ready())
	{
		get_ui_interface()->error(_("Action Not Ready"));
		return 0;
	}

	if(get_instance()->perform_action(action))
		return true;

	get_ui_interface()->error(_("Action Failed."));
	return false;
}

bool
CanvasInterface::add_value_node(synfig::ValueNode::Handle value_node, synfig::String name)
{
	if(name.empty())
	{
		get_ui_interface()->error(_("Empty name!"));
		return false;
	}

	Action::Handle 	action(Action::ValueNodeAdd::create());

	assert(action);
	if(!action)
		return 0;

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("new",value_node);
	action->set_param("name",name);

	if(!action->is_ready())
	{
		get_ui_interface()->error(_("Action Not Ready"));
		return 0;
	}

	if(get_instance()->perform_action(action))
		return true;

	get_ui_interface()->error(_("Action Failed."));
	return false;
}

Action::ParamList
CanvasInterface::generate_param_list(const ValueDesc &value_desc)
{
	synfigapp::Action::ParamList param_list;
	param_list.add("time",get_time());
	param_list.add("canvas_interface",etl::handle<CanvasInterface>(this));
	param_list.add("canvas",get_canvas());

	param_list.add("value_desc",value_desc);

	if(value_desc.parent_is_value_node())
		param_list.add("parent_value_node",value_desc.get_parent_value_node());

	if(value_desc.is_value_node())
		param_list.add("value_node",value_desc.get_value_node());

	if(value_desc.is_const())
	{
		// Fix 1868911: if we put a ValueBase holding a Canvas handle
		// into the param_list and then export the canvas, the handle
		// will miss out of having its reference count reduced,
		// because by the time the handle is destructed the canvas
		// will no longer be inline.  So let's not propagate that
		// ValueBase any further than here.
		if (value_desc.get_value_type() == type_canvas)
			param_list.add("value",Canvas::LooseHandle(value_desc.get_value().get(Canvas::LooseHandle())));
		else
			param_list.add("value",value_desc.get_value());
	}

	if(value_desc.parent_is_layer())
	{
		param_list.add("parent_layer",value_desc.get_layer());
		param_list.add("parent_layer_param",value_desc.get_param_name());
	}

	{
		synfigapp::SelectionManager::ChildrenList children_list;
		children_list=get_selection_manager()->get_selected_children();
		if(!value_desc.parent_is_canvas() && children_list.size()==1)
		{
			param_list.add("dest",value_desc);
			param_list.add("src",children_list.front().get_value_node());
		}
	}
	return param_list;
}

Action::ParamList
CanvasInterface::generate_param_list(const std::list<synfigapp::ValueDesc> &value_desc_list)
{
	synfigapp::Action::ParamList param_list;
	param_list.add("time",get_time());
	param_list.add("canvas_interface",etl::handle<CanvasInterface>(this));
	param_list.add("canvas",get_canvas());

	std::list<synfigapp::ValueDesc>::const_iterator iter;
	for (iter = value_desc_list.begin(); iter != value_desc_list.end(); ++iter) {
		param_list.add("value_desc",*iter);
		if(iter->is_value_node())
		{
			param_list.add("value_node",iter->get_value_node());
		}
	}


	return param_list;
}

void
CanvasInterface::set_rend_desc(const synfig::RendDesc &rend_desc)
{
	Action::Handle 	action(Action::create("CanvasRendDescSet"));

	assert(action);
	if(!action)
		return;

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("rend_desc",rend_desc);

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Action Failed."));
}

void
CanvasInterface::set_name(const synfig::String &x)
{
	Action::Handle 	action(Action::create("CanvasNameSet"));

	assert(action);
	if(!action)
		return;

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("name",x);

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Action Failed."));

	signal_id_changed_();
}

void
CanvasInterface::set_description(const synfig::String &x)
{
	Action::Handle 	action(Action::create("CanvasDescriptionSet"));

	assert(action);
	if(!action)
		return;

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("description",x);

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Action Failed."));
}

void
CanvasInterface::set_id(const synfig::String &x)
{
	Action::Handle 	action(Action::create("CanvasIdSet"));

	assert(action);
	if(!action)
		return;

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("id",x);

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Action Failed."));

	signal_id_changed_();
}


void
CanvasInterface::jump_to_next_keyframe()
{
	synfig::info("Current time: %s",get_time().get_string().c_str());
	KeyframeList::iterator iter;
	if (get_canvas()->keyframe_list().find_next(get_time(), iter)) {
		synfig::Keyframe keyframe(*iter);
		synfig::info("Jumping to keyframe \"%s\" at %s",keyframe.get_description().c_str(),keyframe.get_time().get_string().c_str());
		set_time(keyframe.get_time());
	}
	else {
		synfig::warning("Unable to find next keyframe");
	}
}

void
CanvasInterface::jump_to_prev_keyframe()
{
	synfig::info("Current time: %s",get_time().get_string().c_str());
	KeyframeList::iterator iter;
	//try
	if (get_canvas()->keyframe_list().find_prev(get_time(), iter))
	{
		//synfig::Keyframe keyframe(*get_canvas()->keyframe_list().find_prev(get_time()));
		synfig::Keyframe keyframe(*iter);
		synfig::info("Jumping to keyframe \"%s\" at %s",keyframe.get_description().c_str(),keyframe.get_time().get_string().c_str());
		set_time(keyframe.get_time());
	} else {
		synfig::warning("Unable to find prev keyframe");
	}
	//catch(...) { synfig::warning("Unable to find prev keyframe"); }
}

static void update_layer_size(const RendDesc& rend_desc, Layer::Handle& layer, bool resize_image) {
	int w = layer->get_param("_width").get(int());
	int h = layer->get_param("_height").get(int());

	if (w && h) {
		Vector x, size = rend_desc.get_br() - rend_desc.get_tl();

		// vector from top left of canvas to bottom right
		if (resize_image) {
			if (std::fabs(size[0]) < std::fabs(size[1])) { // if canvas is tall and thin (portrait)
				x[0]=size[0];	// use full width
				x[1]=size[0]/w*h; // and scale for height

				if ((size[0]<0) ^ (size[1]<0))
					x[1] = -x[1];
			} else { // else canvas is short and fat (or maybe square) (landscape)
				x[1]=size[1];	// use full height
				x[0]=size[1]/h*w; // and scale for width

				if ((size[0]<0) ^ (size[1]<0))
					x[0] = -x[0];
			}
		} else {
			x[0] = w*rend_desc.get_pw();
			x[1] = h*rend_desc.get_ph();
		}

		if(!layer->set_param("tl",ValueBase(-x/2)))
			throw int();
		if(!layer->set_param("br",ValueBase(x/2)))
			throw int();
	} else {
		if(!layer->set_param("tl",ValueBase(rend_desc.get_tl())))
			throw int();
		if(!layer->set_param("br",ValueBase(rend_desc.get_br())))
			throw int();
	}
}

Layer::Handle
CanvasInterface::import(
	const synfig::filesystem::Path& filename,
	synfig::String& errors,
	synfig::String& warnings,
	bool resize_image)
{
	Action::PassiveGrouper group(get_instance().get(),_("Import"));

	synfig::info("Attempting to import %s", filename.c_str());
	
	String ext(filename.extension().u8string());
	if (ext.empty()) {
		get_ui_interface()->error(_("File name must have an extension!"));
		return nullptr;
	}

	if (ext.size()) ext = ext.substr(1); // skip initial '.'
	strtolower(ext);

	String short_filename = CanvasFileNaming::make_short_filename(get_canvas()->get_file_name(), filename.u8string());
	String full_filename = CanvasFileNaming::make_full_filename(get_canvas()->get_file_name(), short_filename);

	if (ext=="pgo" || ext=="tsv" || ext=="xml")
	{
		synfigapp::Action::PassiveGrouper group(get_instance().get(),_("Import Lipsync"));

		// switch

		Layer::Handle layer_switch = layer_create("switch", get_canvas());
		if(!layer_switch)
			throw String(_("Unable to create \"Switch\" layer"));

		layer_set_defaults(layer_switch);
		layer_switch->set_description(filename.filename().u8string());

		ValueNode_AnimatedFile::Handle animatedfile_node = ValueNode_AnimatedFile::create(String());
		animatedfile_node->set_link("filename", ValueNode_Const::create(short_filename));
		layer_switch->connect_dynamic_param("layer_name", ValueNode::LooseHandle(animatedfile_node));

		if (!layer_add_action(layer_switch))
			throw String(_("Unable to add \"Switch\" layer"));

		// sound

		String soundfile = animatedfile_node->get_file_field(0, "sound");
		if (!soundfile.empty())
		{
			soundfile = filesystem::Path(full_filename).append(soundfile).u8string();
			String short_soundfile = CanvasFileNaming::make_short_filename(get_canvas()->get_file_name(), soundfile);
			//String full_soundfile = CanvasFileNaming::make_full_filename(get_canvas()->get_file_name(), short_soundfile);

			Layer::Handle layer_sound = layer_create("sound", get_canvas());
			if(!layer_sound)
				throw String(_("Unable to create \"Sound\" layer"));

			layer_set_defaults(layer_sound);
			layer_sound->set_description(filename.filename().u8string());
			layer_sound->set_param("filename", ValueBase(short_soundfile));

			if (!layer_add_action(layer_sound))
				throw String(_("Unable to add \"Sound\" layer"));
		}

		return layer_switch;
	}

	if (ext=="wav" || ext=="ogg" || ext=="mp3")
	{
		Layer::Handle layer = layer_create("sound", get_canvas());
		if(!layer)
			throw String(_("Unable to create \"Sound\" layer"));

		layer_set_defaults(layer);
		layer->set_description(filename.filename().u8string());
		layer->set_param("filename", ValueBase(short_filename));

		if (!layer_add_action(layer))
			throw String(_("Unable to add \"Sound\" layer"));

		return layer;
	}

	if (ext=="svg") //I don't like it, but worse is nothing
	{
		Layer::Handle _new_layer(add_layer_to("group",get_canvas()));
		Layer::Handle _aux_layer(add_layer_to("svg_layer",get_canvas()));
		if(_aux_layer){
			_aux_layer->set_param("filename",ValueBase(short_filename));
			_new_layer->set_param("canvas",ValueBase(_aux_layer->get_param("canvas")));
			//remove aux layer
			Action::Handle 	action(Action::LayerRemove::create());
			assert(action);
			if(!action)
				return 0;
			action->set_param("canvas",get_canvas());
			action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
			action->set_param("layer",_aux_layer);
			if(!action->is_ready()){
				get_ui_interface()->error(_("Action Not Ready"));
				return 0;
			}
			if(!get_instance()->perform_action(action)){
				get_ui_interface()->error(_("Action Failed."));
				return 0;
			}
		}
		signal_layer_new_description()(_new_layer, filename.filename().u8string());
		return _new_layer;
	}

	// If this is a SIF file, then we need to do things slightly differently
	if (ext=="sif" || ext=="sifz")try
	{
		FileSystem::Handle file_system = CanvasFileNaming::make_filesystem(full_filename);
		if(!file_system)
			throw String(_("Unable to open container")) + ":\n\n" + errors;

		Canvas::Handle outside_canvas(synfig::open_canvas_as(file_system->get_identifier(CanvasFileNaming::project_file(full_filename)), full_filename, errors, warnings));
		if(!outside_canvas)
			throw String(_("Unable to open this composition")) + ":\n\n" + errors;

		Layer::Handle layer(add_layer_to("group",get_canvas()));
		if(!layer)
			throw String(_("Unable to create \"Group\" layer"));
		if(!layer->set_param("canvas",ValueBase(outside_canvas)))
			throw int();
		if(!layer->set_param("children_lock",true))
			throw String(_("Could not set children lock of imported canvas"));
		get_canvas()->register_external_canvas(full_filename, outside_canvas);

		//layer->set_description(basename(filename));
		signal_layer_new_description()(layer, filename.filename().u8string());
		return layer;
	}
	catch (const String& x)
	{
		get_ui_interface()->error(filename.u8string() + ": " + x);
		return nullptr;
	}
	catch (...)
	{
		get_ui_interface()->error(_("Uncaught exception when attempting\nto open this composition -- ") + filename.u8string());
		return nullptr;
	}

	if(!Importer::book().count(ext))
	{
		get_ui_interface()->error(_("I don't know how to open images of this type -- ") + ext);
		return nullptr;
	}

	try
	{
		Layer::Handle layer(add_layer_to("Import",get_canvas()));
		if(!layer)
			throw int();
		if(!layer->set_param("filename",ValueBase(short_filename)))
			throw int();
		update_layer_size(get_canvas()->rend_desc(), layer, resize_image);
		layer->monitor(filename);
		String desc = filename.filename().u8string();
		layer->set_description(desc);
		signal_layer_new_description()(layer, desc);
		//get_instance()->set_selected_layer(get_canvas(), layer);
		//get_instance()->set_selected_layer(layer, get_canvas());

		//get_instance()->get_canvas_view(get_canvas());

		//get_selection_manager()->set_selected_layer(layer);

		//Canvas::Handle canvas = get_canvas();
		//CanvasView::Handle view = get_instance()->find_canvas_view(canvas);
		//view->layer_tree->select_layer(layer);

		// add imported layer into switch
		Action::Handle action(Action::create("LayerEncapsulateSwitch"));
		assert(action);
		if(!action) return nullptr;
		action->set_param("canvas",get_canvas());
		action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
		action->set_param("layer",layer);
		action->set_param("description",layer->get_description());
		if(!action->is_ready())
			{ get_ui_interface()->error(_("Action Not Ready")); return nullptr; }
		if(!get_instance()->perform_action(action))
			{ get_ui_interface()->error(_("Action Failed.")); return nullptr; }

		Layer::LooseHandle l = layer->get_parent_paste_canvas_layer(); // get parent layer, because image is incapsulated into action switch
		
		get_selection_manager()->clear_selected_layers();
		get_selection_manager()->set_selected_layer(l);

		return l;
	}
	catch(...)
	{
		get_ui_interface()->error(strprintf(_("Unable to import %s"), filename.u8_str()));
		group.cancel();
		return nullptr;
	}
}


bool
CanvasInterface::import_sequence(
	const std::set<synfig::filesystem::Path>& filenames,
	synfig::String& errors,
	synfig::String& /*warnings*/,
	bool resize_image,
	bool remove_dups)
{
	Action::PassiveGrouper group(get_instance().get(),_("Import sequence"));

	const float fps = get_canvas()->rend_desc().get_frame_rate();

	synfig::info("Attempting to import sequence");
	Layer::Handle layer_switch;
	ValueNode_Animated::Handle layer_name_animated = ValueNode_Animated::create(type_string);

	try {
		// add imported layers into switch
		Action::Handle action(Action::create("LayerEncapsulateSwitch"));
		Action::Handle d_action(synfigapp::Action::LayerRemove::create());
		if(!action)
			{ get_ui_interface()->error(_("Cannot create action")); throw int(); }
		action->set_param("canvas",get_canvas());
		action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
		d_action->set_param("canvas",get_canvas());
		d_action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));

		// create layers and assign them with LayerEncapsulateSwitch action
		Layer::Handle first_imported_layer;
		rendering::Surface::Handle prev_surface;
		int layers_count = 0;
		for (const auto& filename : filenames) {
			synfig::info("Attempting to import '%s' into sequence", filename.u8_str());
			
			String ext(filename.extension().u8string());
			if (!ext.empty()) ext = ext.substr(1); // skip initial '.'
			strtolower(ext);
			
			if (ext.empty())
			{
				errors += synfig::strprintf(_("Cannot import file without extension: %s\n"), filename.u8_str());
				continue;
			}
			
			if(!Importer::book().count(ext))
			{
				errors += synfig::strprintf(_("Cannot import file of type '%s': %s\n"), ext.c_str(), filename.u8_str());
				continue;
			}
			
			String short_filename = CanvasFileNaming::make_short_filename(get_canvas()->get_file_name(), filename.u8string());
			
			try {
				Layer::Handle layer = add_layer_to("Import", get_canvas());
				if (!layer)
					throw int();
				if (!layer->set_param("filename", ValueBase(short_filename)))
					throw int();

				bool is_layer_equal_previous_one = false;

				if (remove_dups) {
					// Gets the surface of current imported image
					auto curr_layer = Layer_Bitmap::Handle::cast_dynamic(layer);
					if (!curr_layer)
						throw int();
					rendering::SurfaceResource::LockRead<rendering::SurfaceSW> cur_lock(curr_layer->rendering_surface);
					if (!cur_lock)
						throw int();
					rendering::Surface::Handle cur_surface = cur_lock.get_handle();

					// Finally checks if it is equal to previous imported frame
					if (prev_surface && cur_surface && cur_surface->equals_to(prev_surface)) {
						is_layer_equal_previous_one = true;
						d_action->set_param("layer", layer);
						synfig::info("\tEquals to previous sequence item");
					} else {
						prev_surface = cur_surface;
					}
				}
				// Do add the new layer and the waypoint to it
				if (!is_layer_equal_previous_one) {
					if (!first_imported_layer)
						first_imported_layer = layer;
					update_layer_size(get_canvas()->rend_desc(), layer, resize_image);
					layer->monitor(filename);
					String desc = filename.filename().u8string();
					layer->set_description(desc);
					signal_layer_new_description()(layer, desc);

					action->set_param("layer", layer);
					if (layers_count == 0)
						action->set_param("description", desc);

					Waypoint& wp = *layer_name_animated->new_waypoint(Time(layers_count/fps), ValueBase(desc));
					wp.set_before(INTERPOLATION_CONSTANT);
					wp.set_after(INTERPOLATION_CONSTANT);
				}

				++layers_count;
			} catch(...) {
				errors += synfig::strprintf(_("Unable to import file: %s"), filename.u8_str());
				group.cancel();
				return false;
			}
		}

		if (!layers_count)
			{ get_ui_interface()->error(_("Nothing imported")); throw int(); }
		if(!action->is_ready())
			{ get_ui_interface()->error(_("Action Not Ready")); throw int(); }
		if(!get_instance()->perform_action(action))
			{ get_ui_interface()->error(_("Action Failed.")); throw int(); }
		if (remove_dups) {
			if (d_action->is_ready())
				if (!get_instance()->perform_action(d_action))
					{ get_ui_interface()->error(_("Delete Action Failed.")); throw int(); }
		}

		if (first_imported_layer) {
			// get parent layer, because image is encapsulated into action switch
			layer_switch = first_imported_layer->get_parent_paste_canvas_layer();
			// connect animated layer_name param
			action = Action::create("LayerParamConnect");
			if(!action)
				{ get_ui_interface()->error(_("Cannot create action")); throw int(); }
			action->set_param("canvas", get_canvas());
			action->set_param("canvas_interface", etl::loose_handle<CanvasInterface>(this));
			action->set_param("layer", layer_switch);
			action->set_param("param", "layer_name");
			action->set_param("value_node", ValueNode::Handle(layer_name_animated));
			if(!action->is_ready())
				{ get_ui_interface()->error(_("Action Not Ready")); throw int(); }
			if(!get_instance()->perform_action(action))
				{ get_ui_interface()->error(_("Action Failed.")); throw int(); }
		}
	} catch(...) {
		get_ui_interface()->error("Unable to import sequence");
		group.cancel();
		return false;
	}

	get_selection_manager()->clear_selected_layers();
	if (layer_switch)
		get_selection_manager()->set_selected_layer(layer_switch);
	return true;
}

void CanvasInterface::waypoint_set_value_node(ValueNode::Handle value_node, const Waypoint& waypoint)
{
	Action::Handle 	action(Action::create("WaypointSetSmart"));

	assert(action);
	if(!action)
		return;

	action->set_param("canvas", get_canvas());
	action->set_param("canvas_interface", etl::loose_handle<CanvasInterface>(this));
	action->set_param("value_node", value_node);
	action->set_param("waypoint", waypoint);
//	action->set_param("time",canvas_interface()->get_time());

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Action Failed."));
}

void CanvasInterface::waypoint_move(const ValueDesc& value_desc, const Time& time, const Time& deltatime)
{
	Action::Handle 	action(Action::create("TimepointsMove"));

	assert(action);
	if(!action)
		return;

	action->set_param("canvas", get_canvas());
	action->set_param("canvas_interface", etl::loose_handle<CanvasInterface>(this));
	if (value_desc.get_value_type() == type_canvas && !getenv("SYNFIG_SHOW_CANVAS_PARAM_WAYPOINTS")) {
		action->set_param("addcanvas", value_desc.get_value().get(Canvas::Handle()));
	} else {
		action->set_param("addvaluedesc", value_desc);
	}

	action->set_param("addtime", time);

	action->set_param("deltatime", deltatime);

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Action Failed."));
}

void
CanvasInterface::waypoint_duplicate(synfigapp::ValueDesc value_desc,synfig::Waypoint waypoint)
{
	//ValueNode::Handle value_node();
	waypoint_duplicate(value_desc.get_value_node(), waypoint);
}

void
CanvasInterface::waypoint_duplicate(ValueNode::Handle value_node,synfig::Waypoint waypoint)
{
	Action::Handle 	action(Action::create("WaypointSetSmart"));

	assert(action);
	if(!action)
		return;

	waypoint.make_unique();
	waypoint.set_time(get_time());

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("waypoint",waypoint);
	action->set_param("time",get_time());
	action->set_param("value_node",value_node);

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Action Failed."));
}

void
CanvasInterface::waypoint_remove(synfigapp::ValueDesc value_desc,synfig::Waypoint waypoint)
{
	//ValueNode::Handle value_node();
	waypoint_remove(value_desc.get_value_node(), waypoint);
}

void
CanvasInterface::waypoint_remove(ValueNode::Handle value_node,synfig::Waypoint waypoint)
{
	Action::Handle 	action(Action::create("WaypointRemove"));

	assert(action);
	if(!action)
		return;

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("waypoint",waypoint);
	action->set_param("value_node",value_node);

	if(!get_instance()->perform_action(action))
		get_ui_interface()->error(_("Action Failed."));
}

bool
CanvasInterface::change_value(synfigapp::ValueDesc value_desc,synfig::ValueBase new_value,bool lock_animation)
{
	return change_value_at_time(value_desc, new_value, get_time(), lock_animation);
}

bool CanvasInterface::change_value_at_time(ValueDesc value_desc, ValueBase new_value, const Time& time, bool lock_animation)
{
	ValueBase old_value;
	old_value = value_desc.get_value(time);

	// If this isn't really a change, then don't bother
	if(new_value==old_value)
		return true;

	// New value should inherit all properties of original ValueBase (static, etc...)
	new_value.copy_properties_of(old_value);

	// If this change needs to take place elsewhere, then so be it.
	if(value_desc.get_canvas())
	{
		if (value_desc.get_canvas()->get_root() != get_canvas()->get_root())
		{
			etl::handle<Instance> instance;
			instance=find_instance(value_desc.get_canvas()->get_root());

			if(instance)
				return instance->find_canvas_interface(value_desc.get_canvas())->change_value_at_time(value_desc,new_value, time);
			else
			{
				get_ui_interface()->error(_("The value you are trying to edit is in a composition\nwhich doesn't seem to be open. Open that composition and you\nshould be able to edit this value as normal."));
				return false;
			}
		}
	}
#ifdef _DEBUG
	else
	{ synfig::warning("Can't get canvas from value desc...?"); }
#endif

	synfigapp::Action::Handle action(synfigapp::Action::create("ValueDescSet"));
	if(!action)
	{
		return false;
	}

	action->set_param("canvas",get_canvas());
	action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
	action->set_param("time",time);
	action->set_param("value_desc",value_desc);
	action->set_param("new_value",new_value);
	if (lock_animation) action->set_param("lock_animation", lock_animation);

	return get_instance()->perform_action(action);
}

void
CanvasInterface::set_meta_data(const synfig::String& key,const synfig::String& data)
{
	if (get_canvas()->get_meta_data(key) == data)
		return;

	if (key=="guide")
	{
		// Create an undoable action

		synfigapp::Action::Handle action(synfigapp::Action::create("CanvasMetadataSet"));

		assert(action);
		if(!action)
			return;

		action->set_param("canvas",get_canvas());
		action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
		action->set_param("key",key);
		action->set_param("value",data);

		get_instance()->perform_action(action);
	}
	else
	{
		get_canvas()->set_meta_data(key,data);
	}
}

void
CanvasInterface::erase_meta_data(const synfig::String& key)
{
	if (key=="guide")
	{
		// Create an undoable action
		synfigapp::Action::Handle action(synfigapp::Action::create("CanvasMetadataErase"));

		assert(action);
		if(!action)
			return;

		action->set_param("canvas",get_canvas());
		action->set_param("canvas_interface",etl::loose_handle<CanvasInterface>(this));
		action->set_param("key",key);

		get_instance()->perform_action(action);
	}
	else
	{
		get_canvas()->erase_meta_data(key);
	}
}

// this function goes with find_important_value_descs()
static int
_process_value_desc(const synfigapp::ValueDesc& value_desc,std::vector<synfigapp::ValueDesc>& out, synfig::GUIDSet& guid_set)
{
	int ret(0);

	if(value_desc.get_value_type()==type_canvas)
	{
		Canvas::Handle canvas;
		canvas=value_desc.get_value().get(canvas);
		if(!canvas || !canvas->is_inline())
			return ret;
		ret+=CanvasInterface::find_important_value_descs(canvas,out,guid_set);
	}

	if(value_desc.is_value_node())
	{
		ValueNode::Handle value_node(value_desc.get_value_node());

		if(guid_set.count(value_node->get_guid()))
			return ret;
		guid_set.insert(value_node->get_guid());

		if(LinkableValueNode::Handle::cast_dynamic(value_node))
		{
			if(ValueNode_DynamicList::Handle::cast_dynamic(value_node))
			{
				out.push_back(value_desc);
				ret++;
			}
			// Process the linkable ValueNode's children
			LinkableValueNode::Handle value_node_copy(LinkableValueNode::Handle::cast_dynamic(value_node));
			for (int i = 0; i < value_node_copy->link_count(); i++) {
				ValueNode::Handle link(value_node_copy->get_link(i));
				if(!link->is_exported())
					ret+=_process_value_desc(ValueDesc(value_node_copy,i),out,guid_set);
			}
		}
		else if(ValueNode_Animated::Handle::cast_dynamic(value_node))
		{
			out.push_back(value_desc);
			ret++;
		}
	}

	return ret;
}

int
CanvasInterface::find_important_value_descs(synfig::Canvas::Handle canvas,std::vector<synfigapp::ValueDesc>& out,synfig::GUIDSet& guid_set)
{
	int ret(0);
	if(!canvas->is_inline())
	{
		ValueNodeList::const_iterator iter;

		for(
			iter=canvas->value_node_list().begin();
			iter!=canvas->value_node_list().end();
			++iter)
			ret+=_process_value_desc(ValueDesc(canvas,(*iter)->get_id()),out,guid_set);
	}

	IndependentContext iter;

	for (iter = canvas->get_independent_context(); iter != canvas->end(); ++iter) {
		Layer::Handle layer(*iter);

		Layer::DynamicParamList::const_iterator iter;
		for(
			iter=layer->dynamic_param_list().begin();
			iter!=layer->dynamic_param_list().end();
			++iter)
		{
			if(!iter->second->is_exported())
				ret+=_process_value_desc(ValueDesc(layer,iter->first),out,guid_set);
		}
		ValueBase value(layer->get_param("canvas"));
		if(value.is_valid())
			ret+=_process_value_desc(ValueDesc(layer,"canvas"),out,guid_set);
	}

	return ret;
}

int
CanvasInterface::find_important_value_descs(std::vector<synfigapp::ValueDesc>& out)
{
	synfig::GUIDSet tmp;
	return find_important_value_descs(get_canvas(),out,tmp);
}

void
CanvasInterface::seek_frame(int frames)
{
	if(!frames)
		return;
	float fps(get_canvas()->rend_desc().get_frame_rate());
	Time newtime(get_time()+(float)frames/fps);
	newtime=newtime.round(fps);

	if(newtime<=get_canvas()->rend_desc().get_time_start())
		newtime=get_canvas()->rend_desc().get_time_start();
	if(newtime>=get_canvas()->rend_desc().get_time_end())
		newtime=get_canvas()->rend_desc().get_time_end();
	set_time(newtime);
}

void
CanvasInterface::seek_time(synfig::Time time)
{
	if(!time)
		return;

	float fps(get_canvas()->rend_desc().get_frame_rate());

	if(time>=synfig::Time::end())
	{
		set_time(get_canvas()->rend_desc().get_time_end());
		return;
	}
	if(time<=synfig::Time::begin())
	{
		set_time(get_canvas()->rend_desc().get_time_start());
		return;
	}

	Time newtime(get_time()+time);
	newtime=newtime.round(fps);

	if(newtime<=get_canvas()->rend_desc().get_time_start())
		newtime=get_canvas()->rend_desc().get_time_start();
	if(newtime>=get_canvas()->rend_desc().get_time_end())
		newtime=get_canvas()->rend_desc().get_time_end();
	set_time(newtime);
}

