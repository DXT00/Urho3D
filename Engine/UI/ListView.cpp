//
// Urho3D Engine
// Copyright (c) 2008-2011 Lasse ��rni
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Precompiled.h"
#include "BorderImage.h"
#include "InputEvents.h"
#include "ListView.h"
#include "UIEvents.h"

#include "DebugNew.h"

static const ShortStringHash indentHash("Indent");

int getItemIndent(UIElement* item)
{
    if (!item)
        return 0;
    return item->getUserData()[indentHash].getInt();
}

ListView::ListView(const std::string& name) :
    ScrollView(name),
    mSelection(M_MAX_UNSIGNED),
    mShowSelectionAlways(false),
    mHierarchyMode(false),
    mDoubleClickInterval(0.5f),
    mDoubleClickTimer(0.0f)
{
    UIElement* container = new UIElement();
    container->setEnabled(true);
    container->setLayout(LM_VERTICAL);
    setContentElement(container);
    
    subscribeToEvent(EVENT_UIMOUSECLICK, EVENT_HANDLER(ListView, handleUIMouseClick));
}

ListView::~ListView()
{
}

void ListView::setStyle(const XMLElement& element, ResourceCache* cache)
{
    ScrollView::setStyle(element, cache);
    
    UIElement* root = getRootElement();
    XMLElement itemElem = element.getChildElement("listitem");
    if (root)
    {
        while (itemElem)
        {
            if (itemElem.hasAttribute("name"))
            {
                UIElement* item = root->getChild(itemElem.getString("name"), true);
                addItem(item);
                if (itemElem.hasAttribute("indent"))
                    item->getUserData()[indentHash] = itemElem.getInt("indent");
                itemElem = itemElem.getNextElement("listitem");
            }
        }
    }
    
    if (element.hasChildElement("selection"))
        setSelection(element.getChildElement("selection").getInt("value"));
    if (element.hasChildElement("showselectionalways"))
        setShowSelectionAlways(element.getChildElement("showselectionalways").getBool("enable"));
    if (element.hasChildElement("hierarchymode"))
        setHierarchyMode(element.getChildElement("hierarchymode").getBool("enable"));
    if (element.hasChildElement("doubleclickinterval"))
        setDoubleClickInterval(element.getChildElement("doubleclickinterval").getFloat("value"));
}

void ListView::update(float timeStep)
{
    if (mDoubleClickTimer > 0.0f)
        mDoubleClickTimer = max(mDoubleClickTimer - timeStep, 0.0f);
}

void ListView::onKey(int key, int buttons, int qualifiers)
{
    // If no selection, can not move with keys
    unsigned numItems = getNumItems();
    
    if ((mSelection != M_MAX_UNSIGNED) && (numItems))
    {
        switch (key)
        {
        case KEY_LEFT:
            if (mHierarchyMode)
            {
                setChildItemsVisible(false);
                return;
            }
            break;
            
        case KEY_RIGHT:
            if (mHierarchyMode)
            {
                setChildItemsVisible(true);
                return;
            }
            break;
            
        case KEY_RETURN:
            if (mHierarchyMode)
            {
                toggleChildItemsVisible();
                return;
            }
            break;
            
        case KEY_UP:
            if (qualifiers & QUAL_CTRL)
                changeSelection(-(int)numItems);
            else
                changeSelection(-1);
            return;
            
        case KEY_DOWN:
            if (qualifiers & QUAL_CTRL)
                changeSelection(numItems);
            else
                changeSelection(1);
            return;
            
        case KEY_PAGEUP:
            {
                // Convert page step to pixels and see how many items we have to skip to reach that many pixels
                int stepPixels = ((int)(mPageStep * mScrollPanel->getHeight())) - getSelectedItem()->getHeight();
                unsigned newSelection = mSelection;
                unsigned okSelection = mSelection;
                while (newSelection < numItems)
                {
                    UIElement* item = getItem(newSelection);
                    int height = 0;
                    if (item->isVisible())
                    {
                        height = item->getHeight();
                        okSelection = newSelection;
                    }
                    if (stepPixels < height)
                        break;
                    stepPixels -= height;
                    --newSelection;
                }
                setSelection(okSelection);
            }
            return;
            
        case KEY_PAGEDOWN:
            {
                int stepPixels = ((int)(mPageStep * mScrollPanel->getHeight())) - getSelectedItem()->getHeight();
                unsigned newSelection = mSelection;
                unsigned okSelection = mSelection;
                while (newSelection < numItems)
                {
                    UIElement* item = getItem(newSelection);
                    int height = 0;
                    if (item->isVisible())
                    {
                        height = item->getHeight();
                        okSelection = newSelection;
                    }
                    if (stepPixels < height)
                        break;
                    stepPixels -= height;
                    ++newSelection;
                }
                setSelection(okSelection);
            }
            return;
            
        case KEY_HOME:
            changeSelection(-(int)getNumItems());
            return;
            
        case KEY_END:
            changeSelection(getNumItems());
            return;
        }
    }
    
    using namespace ListViewKey;
    
    VariantMap eventData;
    eventData[P_ELEMENT] = (void*)this;
    eventData[P_KEY] = key;
    eventData[P_BUTTONS] = buttons;
    eventData[P_QUALIFIERS] = qualifiers;
    sendEvent(EVENT_LISTVIEWKEY, eventData);
}

void ListView::onResize()
{
    ScrollView::onResize();
    
    // Set the content element width to match the scrollpanel
    const IntRect& clipBorder = mScrollPanel->getClipBorder();
    mContentElement->setWidth(mScrollPanel->getWidth() - clipBorder.mLeft - clipBorder.mRight);
}

void ListView::onFocus()
{
    updateSelectionEffect();
}

void ListView::onDefocus()
{
    updateSelectionEffect();
}

void ListView::addItem(UIElement* item)
{
    if ((!item) || (item->getParent() == mContentElement))
        return;
    
    // Enable input so that clicking the item can be detected
    item->setEnabled(true);
    mContentElement->addChild(item);
}

void ListView::removeItem(UIElement* item)
{
    std::vector<UIElement*> items = mContentElement->getChildren();
    for (unsigned i = 0; i < items.size(); ++i)
    {
        if (items[i] == item)
        {
            if (mSelection == i)
                clearSelection();
            else if (mSelection > i)
                changeSelection(-1);
            break;
        }
    }
    mContentElement->removeChild(item);
}

void ListView::removeItem(unsigned index)
{
    if (index >= getNumItems())
        return;
    UIElement* item = mContentElement->getChild(index);
    if (mSelection == index)
        clearSelection();
    else if (mSelection > index)
        changeSelection(-1);
    mContentElement->removeChild(item);
}

void ListView::removeAllItems()
{
    mContentElement->removeAllChildren();
    clearSelection();
}

void ListView::setSelection(unsigned index)
{
    if (index >= getNumItems())
        index = M_MAX_UNSIGNED;
    else
    {
        if (!getItem(index)->isVisible())
            index = M_MAX_UNSIGNED;
    }
    
    mSelection = index;
    updateSelectionEffect();
    ensureItemVisibility();
    
    using namespace ItemSelected;
    
    VariantMap eventData;
    eventData[P_ELEMENT] = (void*)this;
    eventData[P_SELECTION] = mSelection;
    sendEvent(EVENT_ITEMSELECTED, eventData);
}

void ListView::changeSelection(int delta)
{
    if (mSelection == M_MAX_UNSIGNED)
        return;
    
    unsigned numItems = getNumItems();
    unsigned newSelection = mSelection;
    unsigned okSelection = mSelection;
    while (delta != 0)
    {
        if (delta > 0)
        {
            ++newSelection;
            if (newSelection >= numItems)
                break;
        }
        if (delta < 0)
        {
            --newSelection;
            if (newSelection >= numItems)
                break;
        }
        UIElement* item = getItem(newSelection);
        if (item->isVisible())
        {
            okSelection = newSelection;
            if (delta > 0)
                --delta;
            if (delta < 0)
                ++delta;
        }
    }
    
    setSelection(okSelection);
}

void ListView::clearSelection()
{
    setSelection(M_MAX_UNSIGNED);
}

void ListView::setShowSelectionAlways(bool enable)
{
    mShowSelectionAlways = enable;
}

void ListView::setHierarchyMode(bool enable)
{
    mHierarchyMode = enable;
}

void ListView::setDoubleClickInterval(float interval)
{
    mDoubleClickInterval = interval;
}

void ListView::setChildItemsVisible(bool enable)
{
    if ((!mHierarchyMode) || (mSelection == M_MAX_UNSIGNED))
        return;
    
    unsigned numItems = getNumItems();
    int baseIndent = getItemIndent(getSelectedItem());
    
    for (unsigned i = mSelection + 1; i < numItems; ++i)
    {
        UIElement* item = getItem(i);
        if (getItemIndent(item) > baseIndent)
            item->setVisible(enable);
        else
            break;
    }
}

void ListView::toggleChildItemsVisible()
{
    if ((!mHierarchyMode) || (mSelection == M_MAX_UNSIGNED))
        return;
    
    unsigned numItems = getNumItems();
    int baseIndent = getItemIndent(getSelectedItem());
    
    for (unsigned i = mSelection + 1; i < numItems; ++i)
    {
        UIElement* item = getItem(i);
        if (getItemIndent(item) > baseIndent)
            item->setVisible(!item->isVisible());
        else
            break;
    }
}

unsigned ListView::getNumItems() const
{
    return mContentElement->getNumChildren();
}

UIElement* ListView::getItem(unsigned index) const
{
    return mContentElement->getChild(index);
}

std::vector<UIElement*> ListView::getItems() const
{
    return mContentElement->getChildren();
}

UIElement* ListView::getSelectedItem() const
{
    return mContentElement->getChild(mSelection);
}

void ListView::updateSelectionEffect()
{
    unsigned numItems = getNumItems();
    for (unsigned i = 0; i < numItems; ++i)
        getItem(i)->setSelected((i == mSelection) && ((mFocus) || (mShowSelectionAlways)));
}

void ListView::ensureItemVisibility()
{
    UIElement* selected = getSelectedItem();
    if (!selected)
        return;
    
    IntVector2 currentOffset = selected->getScreenPosition() - mScrollPanel->getScreenPosition() - mContentElement->getPosition();
    IntVector2 newView = getViewPosition();
    const IntRect& clipBorder = mScrollPanel->getClipBorder();
    IntVector2 windowSize(mScrollPanel->getWidth() - clipBorder.mLeft - clipBorder.mRight, mScrollPanel->getHeight() -
        clipBorder.mTop - clipBorder.mBottom);
    
    if (currentOffset.mY < 0)
        newView.mY += currentOffset.mY;
    if (currentOffset.mY + selected->getHeight() > windowSize.mY)
        newView.mY += currentOffset.mY + selected->getHeight() - windowSize.mY;
    
    setViewPosition(newView);
}

void ListView::handleUIMouseClick(StringHash eventType, VariantMap& eventData)
{
    if (eventData[UIMouseClick::P_BUTTON].getInt() != MOUSEB_LEFT)
        return;
    
    UIElement* element = static_cast<UIElement*>(eventData[UIMouseClick::P_ELEMENT].getPtr());
    
    unsigned numItems = getNumItems();
    for (unsigned i = 0; i < numItems; ++i)
    {
        if (element == getItem(i))
        {
            bool isDoubleClick = false;
            if ((mDoubleClickTimer > 0.0f) && (mSelection == i))
            {
                isDoubleClick = true;
                mDoubleClickTimer = 0.0f;
            }
            else
                mDoubleClickTimer = mDoubleClickInterval;
            
            setSelection(i);
            
            if (isDoubleClick)
            {
                if (mHierarchyMode)
                    toggleChildItemsVisible();
                
                VariantMap eventData;
                eventData[ItemDoubleClicked::P_ELEMENT] = (void*)this;
                eventData[ItemDoubleClicked::P_SELECTION] = mSelection;
                sendEvent(EVENT_ITEMDOUBLECLICKED, eventData);
            }
            
            return;
        }
    }
}
