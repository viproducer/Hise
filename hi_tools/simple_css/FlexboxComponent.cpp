/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

namespace hise
{
namespace simple_css
{
void FlexboxComponent::Helpers::setFallbackStyleSheet(Component& c, const String& properties)
{
	static const Identifier sid("style");

	if(c.getProperties().contains(sid))
		return;

	c.getProperties().set(sid, properties);
	invalidateCache(c);
}

void FlexboxComponent::Helpers::writeSelectorsToProperties(Component& c, const StringArray& selectors)
{
	Array<Selector> classSelectors;

	String id;

	for(const auto& s: selectors)
	{
		Selector sel(s);

		if(sel.type == SelectorType::Class)
			classSelectors.add(sel);

		if(sel.type == SelectorType::ID)
			id = sel.name;
	}

	static const Identifier iid("id");

	writeClassSelectors(c, classSelectors, false);

	if(id.isNotEmpty())
		c.getProperties().set(iid, id);
}

Selector FlexboxComponent::Helpers::getTypeSelectorFromComponentClass(Component* c)
{
	if(dynamic_cast<Button*>(c) != nullptr)
		return Selector(ElementType::Button);
	if(auto st = dynamic_cast<SimpleTextDisplay*>(c))
		return Selector(st->s);
	if(dynamic_cast<ComboBox*>(c) != nullptr)
		return Selector(ElementType::Selector);
	if(auto vp = dynamic_cast<FlexboxViewport*>(c))
		return getTypeSelectorFromComponentClass(&vp->content);
	if(auto fc = dynamic_cast<FlexboxComponent*>(c))
	{
		if(fc->selector.type == SelectorType::Type)
			return fc->selector;
		else
			return Selector(ElementType::Panel);
	}
	if(dynamic_cast<juce::TextEditor*>(c) != nullptr)
		return Selector(ElementType::TextInput);
    if(dynamic_cast<juce::TableListBox*>(c) != nullptr)
        return Selector(ElementType::Table);
	if(dynamic_cast<juce::TableHeaderComponent*>(c) != nullptr)
		return Selector(ElementType::TableHeader);
	if(dynamic_cast<juce::ProgressBar*>(c) != nullptr)
		return Selector(ElementType::Progress);

	static const Identifier ct("custom-type");

	if(c->getProperties().contains(ct))
	{
		return Selector(c->getProperties()[ct].toString());
	}

	return Selector(ElementType::Panel);;
}

Array<Selector> FlexboxComponent::Helpers::getClassSelectorFromComponentClass(Component* c)
{
	if(auto vp = dynamic_cast<FlexboxViewport*>(c))
		return getClassSelectorFromComponentClass(&vp->content);

	Array<Selector> list;

	static const Identifier cid("class");

	auto classes = c->getProperties()[cid];

	if(classes.isString())
		list.add(Selector(SelectorType::Class, classes.toString()));
	else if(auto a = classes.getArray())
	{
		for(const auto& v: *a)
			list.add(Selector(SelectorType::Class, v.toString()));
	}

	return list;
}

String FlexboxComponent::Helpers::dump(Component& c)
{
    String s;
    
    auto t = getTypeSelectorFromComponentClass(&c);
    
    if(t)
        s << t.toString();
    
    s << " " << getIdSelectorFromComponentClass(&c).toString();

    for(auto c: getClassSelectorFromComponentClass(&c))
        s << " " << c.toString();
    
    return s;
}

void FlexboxComponent::Helpers::writeClassSelectors(Component& c, const Array<Selector>& classList, bool append)
{
	Array<var> classes;

	static const Identifier cid("class");

	if(append)
	{
		if(auto ar = c.getProperties()[cid].getArray())
			classes.addArray(*ar);
	}
	
	for(const auto& s: classList)
		classes.add(s.toString().substring(1, 1000));

	c.getProperties().set(cid, classes);

	invalidateCache(c);
}

Selector FlexboxComponent::Helpers::getIdSelectorFromComponentClass(Component* c)
{
	static const Identifier iid("id");

	if(auto vp = dynamic_cast<FlexboxViewport*>(c))
		return getIdSelectorFromComponentClass(&vp->content);

	auto id = c->getProperties()[iid].toString();

	if(id.isNotEmpty())
		return Selector(SelectorType::ID, id);
	else
		return {};
}

void FlexboxComponent::SimpleTextDisplay::setText(const String& text)
{
	if(currentText != text)
	{
		currentText = text;

		if(auto r = findParentComponentOfClass<CSSRootComponent>())
		{
			dynamic_cast<Component*>(r)->resized();
		}

		repaint();
	}
}

void FlexboxComponent::SimpleTextDisplay::paint(Graphics& g)
{
	if(auto root = findParentComponentOfClass<CSSRootComponent>())
	{
		if(auto ss = root->css.getForComponent(this))
		{
			Renderer r(this, root->stateWatcher);

			auto tb = getLocalBounds().toFloat();
			root->stateWatcher.checkChanges(this, ss, 0);
			r.drawBackground(g, tb, ss);
			r.renderText(g, tb.reduced(2.0f, 0.0f), currentText, ss);
		}
	}
}

FlexboxComponent::FlexboxComponent(const Selector& s):
	selector(s)
{
	setOpaque(false);

	if(s.type != SelectorType::Type)
		Helpers::writeSelectorsToProperties(*this, { s.toString() });

	setRepaintsOnMouseActivity(true);
}

void FlexboxComponent::setCSS(StyleSheet::Collection& css)
{
	ss = css.getForComponent(this);

	childSheets.clear();

	for(int i = 0; i < getNumChildComponents(); i++)
	{
		auto c = getChildComponent(i);

		if(auto sheet = css.getForComponent(c))
		{
			childSheets[c] = sheet;
			c->setMouseCursor(sheet->getMouseCursor());
		}

		if(auto div = dynamic_cast<FlexboxContainer*>(c))
			div->setCSS(css);
	}

	rebuildLayout();
	repaint();
}

void FlexboxComponent::setDefaultStyleSheet(const String& css)
{
	Helpers::setFallbackStyleSheet(*this, css);
}


void FlexboxComponent::paint(Graphics& g)
{
	//g.fillAll(Colour::fromHSL(Random::getSystemRandom().nextFloat(), 0.5, 0.5, 0.1f));

	if(ss != nullptr)
	{
		if(parentToUse == nullptr)
			setParent(CSSRootComponent::find(*this));
		
		if(parentToUse != nullptr)
		{
			Renderer r(this, parentToUse->stateWatcher);
			parentToUse->stateWatcher.checkChanges(this, ss, r.getPseudoClassState());

			auto b = getLocalBounds().toFloat();
			//b = ss->getBounds(b, PseudoState(r.getPseudoClassState()));

			r.setApplyMargin(applyMargin);
			r.drawBackground(g, b, ss);
		}
	}
}

void FlexboxComponent::resized()
{
	if(getLocalBounds().isEmpty())
		return;

	if(isInvisibleWrapper())
	{
		getChildComponent(0)->setBounds(getLocalBounds());
		return;
	}

	auto data = createPositionData();

	std::vector<std::pair<Component*, Rectangle<int>>> prevPositions;

	if(fullRebuild)
	{
		for(int i = 0; i < getNumChildComponents(); i++)
		{
			auto c = getChildComponent(i);

			if(!c->isVisible())
				continue;

			if(auto f = dynamic_cast<FlexboxContainer*>(c))
				prevPositions.push_back({ c, c->getLocalBounds()});
		}
	}
	

	data.flexBox.performLayout(data.area);

	for(auto& ab: data.absolutePositions)
	{
		ab.first->toFront(false);
		ab.first->setBounds(ab.second);
	}

	for(const auto& p: prevPositions)
	{
		if(p.first->getLocalBounds() == p.second)
			p.first->resized();
	}

	fullRebuild = false;
}

void FlexboxComponent::addSpacer()
{
	auto c = new Component();
	Helpers::writeSelectorsToProperties(*c, { ".spacer"} );
	Helpers::setFallbackStyleSheet(*c, "flex-grow: 1;");
	addFlexItem(*c);
	spacers.add(c);
}

void FlexboxComponent::addFlexItem(Component& c)
{
	addAndMakeVisible(c);
}

float FlexboxComponent::getAutoWidthForHeight(float fullHeight)
{
	if(isInvisibleWrapper())
	{
		auto item = createFlexItemForInvisibleWrapper({0.0f, 0.0f, 0.0f, fullHeight});

		auto w = item.width;
		if(item.minWidth > 0.0f)
			w = jmax(w, item.minWidth);

		if(item.maxWidth > 0.0f)
			w = jmin(w, item.maxWidth);

		return w;
	}

	auto data = createPositionData();
    auto w = 0.0f;
    auto first = true;
    
    for(const auto& i: data.flexBox.items)
    {
        w += i.width;
        w += jmax(i.margin.left, i.margin.right);
    }

    if(ss != nullptr)
    {
        Rectangle<float> b(0.0f, 0.0f, w, 0.0f);

        w += ss->getPixelValue(b, {"padding-left", {}});
        w += ss->getPixelValue(b, {"padding-right", {}});

        if(applyMargin)
        {
            w += ss->getPixelValue(b, {"margin-left", {}});
            w += ss->getPixelValue(b, {"margin-right", {}});
        }
    }

    return w;
}

float FlexboxComponent::getAutoHeightForWidth(float fullWidth)
{
	if(isInvisibleWrapper())
	{
		auto item = createFlexItemForInvisibleWrapper({0.0f, 0.0f, fullWidth, 0.0f});

		auto h = item.height;
		if(item.minHeight > 0.0f)
			h = jmax(h, item.minHeight);

		if(item.maxWidth > 0.0f)
			h = jmin(h, item.maxHeight);

		return h;
	}

    float h = 0.0f;

    auto wrap = ss != nullptr ? ss->getAsEnum({ "flex-wrap", {}}, FlexBox::Wrap::noWrap) : FlexBox::Wrap::noWrap;
    auto direction = ss != nullptr ? ss->getAsEnum({ "flex-direction", {}}, FlexBox::Direction::row) : FlexBox::Direction::row;

    if(wrap == FlexBox::Wrap::wrap || wrap == FlexBox::Wrap::wrapReverse)
    {
        if(direction == FlexBox::Direction::column ||
           direction == FlexBox::Direction::columnReverse)
        {
            setSize(getWidth(), 1000);
            resized();

            auto minHeight = 0;
            auto maxHeight = 0;

            for(int i = 0; i < getNumChildComponents(); i++)
            {
                auto child = getChildComponent(i);

                if(!child->isVisible())
                    continue;

                minHeight = jmin(minHeight, child->getBoundsInParent().getY());
                maxHeight = jmax(maxHeight, child->getBoundsInParent().getBottom());
            }

            h = maxHeight - minHeight;
        }
        else
        {
            auto d = createPositionData();
            d.area.setWidth(fullWidth);

            d.flexBox.performLayout(d.area);

            auto minHeight = 0;
            auto maxHeight = 0;

            for(int i = 0; i < getNumChildComponents(); i++)
            {
                auto child = getChildComponent(i);

                if(!child->isVisible())
                    continue;

                minHeight = jmin(minHeight, child->getBoundsInParent().getY());
                maxHeight = jmax(maxHeight, child->getBoundsInParent().getBottom());
            }

            h = maxHeight - minHeight;
        }
    }
    else
    {
        float gap = 0.0f;

        if(ss != nullptr)
        {
            auto mv = ss->getPropertyValueString({"gap", {}});

            if(mv.isNotEmpty())
            {
                ExpressionParser::Context cb;
                cb.fullArea = getLocalBounds().toFloat();
                cb.useWidth = true;
                cb.defaultFontSize = 16.0f;
                gap = ExpressionParser::evaluate(mv, cb);
            }
        }

        auto getHeightFromItem = [](const FlexItem& i)
        {
            auto h = i.height;

            if(i.minHeight > 0.0f)
                h = jmax(i.minHeight, h);

            if(i.maxHeight > 0.0f)
                h = jmin(i.maxHeight, h);

            return h;
        };

        if(direction == FlexBox::Direction::column ||
           direction == FlexBox::Direction::columnReverse)
        {
            auto lastComponent = getFirstLastComponents().second;

            for(int i = 0; i < getNumChildComponents(); i++)
            {
                auto child = getChildComponent(i);

                if(!child->isVisible())
                    continue;

                

                if(auto ssChild = childSheets[child])
                    h += (float)getHeightFromItem(ssChild->getFlexItem(child, getLocalBounds().toFloat()));
                
                if(child != lastComponent)
                    h += gap;
            }
        }
        else
        {
            for(int i = 0; i < getNumChildComponents(); i++)
            {
                auto child = getChildComponent(i);

                if(!child->isVisible())
                    continue;

                if(auto ssChild = childSheets[child])
                    h = jmax(h, getHeightFromItem(ssChild->getFlexItem(child, getLocalBounds().toFloat())));
            }
        }
    }

    if(ss != nullptr)
    {
        Rectangle<float> b(0.0f, 0.0f, 0.0f, h);

        if(applyMargin)
        {
            h += ss->getPixelValue(b, {"margin-top", {}});
            h += ss->getPixelValue(b, {"margin-bottom", {}});
        }
        
        h += ss->getPixelValue(b, {"padding-top", {}});
        h += ss->getPixelValue(b, {"padding-bottom", {}});
    }

    return h;
}



void FlexboxComponent::setParent(CSSRootComponent* p)
{
	parentToUse = p;
}

void FlexboxComponent::setIsInvisibleWrapper(bool shouldBeInvisibleWrapper)
{
	if(invisibleWrapper == shouldBeInvisibleWrapper)
		return;

	invisibleWrapper = shouldBeInvisibleWrapper;

	if(invisibleWrapper)
	{
		jassert(getNumChildComponents() == 1);
		
		StringArray selectors;
		selectors.add(Helpers::getIdSelectorFromComponentClass(this).toString());

		for(auto c: Helpers::getClassSelectorFromComponentClass(this))
			selectors.add(c.toString());

		selector = Selector(ElementType::Panel);

		Helpers::writeSelectorsToProperties(*getChildComponent(0), selectors);

		getProperties().remove("id");

		Helpers::writeSelectorsToProperties(*this, {});
		
		Helpers::writeInlineStyle(*this, "display: flex; gap: 0px; width: auto; height: auto;");
	}
}

FlexboxComponent::PositionData FlexboxComponent::createPositionData()
{
	PositionData data;

	auto b = getLocalBounds().toFloat();
		
	if(ss != nullptr)
	{
		auto isAuto = ss->getPropertyValueString({"height", {}}) == "auto";
		
#if 0
		if(!isAuto)
			b = ss->getBounds(b, {});
#endif

		if(applyMargin)
			b = ss->getArea(b, { "margin", {}});

		b = ss->getArea(b, { "padding", {}});
	}
		
	data.area = b;

	if(ss != nullptr)
	{
		data.flexBox = ss->getFlexBox();

		FlexItem::Margin margin;

		auto mv = ss->getPropertyValueString({"gap", {}});

		if(mv.isNotEmpty())
		{
			ExpressionParser::Context cb;
			cb.fullArea = b;
			cb.useWidth = true;
			cb.defaultFontSize = 16.0f;
			margin = FlexItem::Margin(ExpressionParser::evaluate(mv, cb) * 0.5f);
		}

		auto flc = getFirstLastComponents();

		

		for(int i = 0; i < getNumChildComponents(); i++)
		{
			auto c = getChildComponent(i);
			auto isLast = c == flc.second;
			auto isFirst = c == flc.first;

			if(!c->isVisible())
				continue;

			c->getProperties().set("first-child", isFirst);
			c->getProperties().set("last-child", isLast);

			auto thisMargin = margin;

			if(isFirst)
			{
				if(data.flexBox.flexDirection == FlexBox::Direction::column ||
				   data.flexBox.flexDirection == FlexBox::Direction::columnReverse)
				{
					thisMargin.top = 0.0f;
				}
				else
				{
					thisMargin.left = 0.0f;
				}
					
			}
			if(isLast)
			{
				if(data.flexBox.flexDirection == FlexBox::Direction::column ||
					data.flexBox.flexDirection == FlexBox::Direction::columnReverse)
				{
					thisMargin.bottom = 0.0f;
				}
				else
				{
					thisMargin.right = 0.0f;
				}
			}

			if(auto ptr = childSheets[c])
			{
				auto childPos = ptr->getPositionType({});
	                
				if(childPos == PositionType::absolute || childPos == PositionType::fixed)
				{
					auto absoluteBounds = ptr->getBounds(b, {}).toNearestInt();
					data.absolutePositions.push_back({ c, absoluteBounds });
				}
				else
				{
					data.flexBox.items.add(ptr->getFlexItem(c, data.area).withMargin(thisMargin));
				}
			}
			else
				data.flexBox.items.add(FlexItem(*c).withMargin(thisMargin));
		}
	}

	return data;
}

void HeaderContentFooter::setFixStyleSheet(StyleSheet::Collection& newCss)
{
	if(auto dp = createDataProvider())
	{
		newCss.performAtRules(dp);
		delete dp;
	}
		
	css = newCss;
	useFixStyleSheet = true;

	if(defaultProperties != nullptr)
	{
		for(const auto& p: defaultProperties->getProperties())
			css.setPropertyVariable(p.name, p.value);
	}

	css.setAnimator(&animator);
	currentLaf = new StyleSheetLookAndFeel(*this);
	setLookAndFeel(currentLaf);

	styleSheetCollectionChanged();
}

void HeaderContentFooter::showEditor()
{
	Component::SafePointer<HeaderContentFooter> safeThis(this);

	Editor::showEditor(this, [safeThis](simple_css::StyleSheet::Collection& c)
	{
		if(safeThis.getComponent() != nullptr)
		{
			safeThis->update(c);
			//safeThis->showInfo(true);
		}
			
	});
}

void HeaderContentFooter::setStylesheetCode(const String& code)
{
	simple_css::Parser p(code);
	p.parse();
	auto newCss = p.getCSSValues();
	update(newCss);
}

void hise::simple_css::HeaderContentFooter::paintOverChildren(Graphics& g)
{
	if(!inspectorData.first.isEmpty())
	{
		auto cb = inspectorData.first;
		auto b = getLocalBounds().toFloat();
		auto left = b.removeFromLeft(cb.getX());
		auto right = b.removeFromRight(b.getRight() - cb.getRight());
		auto top = b.removeFromTop(cb.getY());
		auto bottom = b.removeFromBottom(b.getBottom() - cb.getBottom());
            
		g.setColour(Colours::black.withAlpha(0.5f));
		g.fillRect(top);
		g.fillRect(left);
		g.fillRect(right);
		g.fillRect(bottom);
            
		g.setColour(Colour(SIGNAL_COLOUR).withAlpha(0.3f));
		g.drawRect(inspectorData.first, 1.0f);
		g.setColour(Colour(SIGNAL_COLOUR));
		auto f = GLOBAL_MONOSPACE_FONT();
		g.setFont(f);
            
		auto tb = inspectorData.first.withSizeKeepingCentre(f.getStringWidthFloat(inspectorData.second), inspectorData.first.getHeight() + 40).constrainedWithin(getLocalBounds().toFloat());
            
		if(inspectorData.first.getY() > 20)
			g.drawText(inspectorData.second, tb, Justification::centredTop);
		else
			g.drawText(inspectorData.second, tb, Justification::centredBottom);

        if(inspectorData.c.getComponent() == nullptr)
            return;

        if(auto ss = css.getForComponent(inspectorData.c.getComponent()))
        {
	        auto b = inspectorData.first;

            auto mb = ss->getArea(b, { "margin", {}});
            auto pb = ss->getArea(mb, { "padding", {}});

            Colour paddingColour(0xFFB8C37F);
            Colour marginColour(0xFFB08354);

	        {
                Graphics::ScopedSaveState sss(g);
                g.reduceClipRegion(b.toNearestInt());
                g.excludeClipRegion(mb.toNearestInt());
                g.fillAll(marginColour.withAlpha(0.33f));
            }

            
            ;
            {
	        	Graphics::ScopedSaveState sss(g);
                g.reduceClipRegion(mb.toNearestInt());
		        g.excludeClipRegion(pb.toNearestInt());
                g.fillAll(paddingColour.withAlpha(0.33f));
	        }

            g.setColour(paddingColour);
            g.drawRect(mb, 1);
            g.drawRect(pb, 1);
            g.setColour(marginColour);
            g.drawRect(b, 1);

        }
	}
}

void HeaderContentFooter::update(simple_css::StyleSheet::Collection& newCss)
{
	if(useFixStyleSheet)
		css.clearCache();

	if(css != newCss && !useFixStyleSheet)
	{
		if(auto dp = createDataProvider())
		{
			newCss.performAtRules(dp);
			delete dp;
		}

		css = newCss;

		if(defaultProperties != nullptr)
		{
			for(const auto& p: defaultProperties->getProperties()) 
				css.setPropertyVariable(p.name, p.value);
		}

		css.setAnimator(&animator);
		currentLaf = new simple_css::StyleSheetLookAndFeel(*this);
		setLookAndFeel(currentLaf);

		styleSheetCollectionChanged();
	}

	body.setCSS(css);
}

void HeaderContentFooter::resized()
{
	body.setBounds(getLocalBounds());
	body.resized();
	body.repaint();
}


}
}

