/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tutorial_gui.cpp GUI for tutorial window. */

#include "stdafx.h"
#include "tutorial_gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "widgets/tutorial_widget.h"
#include "gfx_func.h"
#include "settings_type.h"
#include "strings_func.h"
#include "string_func.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

struct TutorialContentItem{
	enum TutorialContentType{
	   TEXT,
	   IMAGE,
	   TITLE,//小标题
	   SPACER
};
    TutorialContentType type;
    StringID text_id;
    SpriteID sprite_id;  // 使用 Sprite ID 而非文件路径
    int height;

};
struct TutorialPages 
{
	uint index=0;
	StringID page_title_id;//页面标题
	std::string image_path;
	std::vector<TutorialContentItem> content_items;//页面所有内容的容器
};
struct Tutorial_widgets_disabled_state
{
	bool previous_disabled=true;
	bool next_disabled=false;
};

/** Tutorial window structure */
struct TutorialWindow : public Window {
	std::vector<TutorialPages> tutorial_pages{};
	uint current_page_index = 0;
	Scrollbar *vscroll=nullptr;
	Tutorial_widgets_disabled_state disabled_state{true,false};

	TutorialWindow(WindowDesc &desc) : Window(desc)
	{
		this->CreateNestedTree();
		this->FinishInitNested(0);
		this->vscroll = this->GetScrollbar(WID_TUT_SCROLLBAR);
		LoadPagesFromStrings();
		UpdateScrollbar();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TUT_PREVIOUS:
				// Show previous page
				    if (disabled_state.previous_disabled) 
					{
						return;
					}
				    current_page_index-=1;
				    UpdateUIForPage(current_page_index);
				    break;
			
			case WID_TUT_NEXT:
				//Show next page
				if(disabled_state.next_disabled) 
				{
					return;
				}
				current_page_index +=1;
				UpdateUIForPage(current_page_index);
				break;
			
			case WID_TUT_CLOSE:
			case WID_TUT_FINISH:
				this->Close();
				break;
			
			case WID_TUT_DONT_SHOW:
				_settings_client.gui.tutorial_completed = true;
				WindowDesc::SaveToConfig();
				this->Close();
				break;
		}
	}

	void OnMouseWheel(int wheel, [[maybe_unused]] WidgetID widget) override
	{
		if(this->vscroll==nullptr)return; // 滚动条未初始化
		this->vscroll->UpdatePosition(wheel);
		this->SetWidgetDirty(WID_TUT_CONTENT);
	}

void DrawWidget(const Rect&r,WidgetID widget)const override
{
	if(widget != WID_TUT_CONTENT && widget != WID_TUT_PAGE_INDICATOR) return;
	if (current_page_index >= tutorial_pages.size()) return;
	
	if (widget == WID_TUT_CONTENT) {
		const TutorialPages &page = tutorial_pages[current_page_index];
		Rect text_rect = r.Shrink(5);
		
		// 获取像素级滚动偏移
		int scroll_offset = (this->vscroll != nullptr) ? this->vscroll->GetPosition() : 0;
		int y = text_rect.top - scroll_offset;
		
		// 遍历内容项并绘制
		for (const auto &item : page.content_items) {
			if (y >= text_rect.bottom) break; // 超出可见区域底部
			
			switch (item.type) {
				case TutorialContentItem::TITLE: {
					int title_height = GetCharacterHeight(FS_LARGE);
					if (y + title_height > text_rect.top) {
						DrawString(text_rect.left, text_rect.right, std::max(y, text_rect.top), item.text_id, TC_BLACK, SA_HOR_CENTER, false, FS_LARGE);
					}
					y += title_height + 5;
					break;
				}
				
				case TutorialContentItem::TEXT: {
					int text_height = GetStringHeight(item.text_id, text_rect.Width());
					if (y + text_height > text_rect.top) {
						DrawStringMultiLine(text_rect.left, text_rect.right, std::max(y, text_rect.top), text_rect.bottom, item.text_id, TC_BLACK, SA_LEFT, false, FS_NORMAL);
					}
					y += text_height + 3;
					break;
				}
				
				case TutorialContentItem::IMAGE: {
					// 绘制 Sprite 图片或占位符
					int img_height;
					if (item.sprite_id != 0) {
						Dimension sprite_dim = GetSpriteSize(item.sprite_id);
						img_height = sprite_dim.height;
						if (y + img_height > text_rect.top) {
							int sprite_x = text_rect.left + (text_rect.Width() - sprite_dim.width) / 2; // 居中
							int sprite_y = std::max(y, text_rect.top);
							DrawSprite(item.sprite_id, PAL_NONE, sprite_x, sprite_y);
						}
					} else {
						// 占位符（如果没有 sprite）
						img_height = 100;
						if (y + img_height > text_rect.top) {
							Rect img_rect = {text_rect.left, std::max(y, text_rect.top), text_rect.right, std::min(y + img_height, text_rect.bottom)};
							GfxFillRect(img_rect, PC_GREY, FILLRECT_RECOLOUR);
							int mid_y = img_rect.top + img_rect.Height() / 2;
							if (mid_y >= img_rect.top && mid_y < img_rect.bottom) {
								DrawString(img_rect.left, img_rect.right, mid_y, "[No Image]", TC_BLACK, SA_HOR_CENTER);
							}
						}
					}
					y += img_height + 10;
					break;
				}
				
				case TutorialContentItem::SPACER:
					y += item.height > 0 ? item.height : 10;
					break;
			}
		}
	}
	
	// 绘制页码指示器
	if (widget == WID_TUT_PAGE_INDICATOR) {
		DrawString(r.left, r.right, r.top, GetString(STR_TUTORIAL_PAGE_INDICATOR, current_page_index + 1, tutorial_pages.size()), TC_BLACK, SA_HOR_CENTER);
	}
}

	int CalculateContentHeight() const
	{
		if (current_page_index >= tutorial_pages.size()) return 0;
		const TutorialPages &page = tutorial_pages[current_page_index];
		
		int content_height = 0;
		int content_width = 390; // 默认宽度
		
		// 遍历所有内容项计算高度
		for (const auto &item : page.content_items) {
			switch (item.type) {
				case TutorialContentItem::TITLE:
					content_height += GetCharacterHeight(FS_LARGE) + 5;
					break;
					
				case TutorialContentItem::TEXT: {
					int text_height = GetStringHeight(item.text_id, content_width);
					content_height += text_height + 3;
					break;
				}
				
				case TutorialContentItem::IMAGE:
					if (item.sprite_id != 0) {
						Dimension sprite_dim = GetSpriteSize(item.sprite_id);
						content_height += sprite_dim.height + 10; // 10 像素间距
					} else {
						content_height += 100 + 10; // 占位符高度
					}
					break;
				
				case TutorialContentItem::SPACER:
					content_height += item.height > 0 ? item.height : 10;
					break;
			}
		}
		
		// 添加底部间距
		content_height += 20;
		
		return content_height;
	}
	
	/** 更新滚动条状态 */
	void UpdateScrollbar()
	{
		if (this->vscroll == nullptr) return; // 滚动条未初始化
		
		int visible_height = 300; // 默认值（用于初始化阶段）
		const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_TUT_CONTENT);
		if (wid != nullptr) {
			visible_height = wid->current_y; // 使用实际高度
		}
		
		int content_height = CalculateContentHeight();
		
		// 按像素设置滚动条
		this->vscroll->SetCount(content_height);
		this->vscroll->SetCapacity(visible_height);
		this->vscroll->SetStepSize(GetCharacterHeight(FS_NORMAL)); // 每次滚动一行的高度
	}


void LoadPagesFromStrings()
{
	tutorial_pages.clear();
	for(uint i = 0;i<6; i++) {
		StringID title_id = static_cast<StringID>(STR_TUTORIAL_PAGE_1_TITLE + i * 2);
		StringID body_id = static_cast<StringID>(STR_TUTORIAL_PAGE_1_BODY + i * 2);
		
		TutorialPages page;
		page.index = i;
		page.page_title_id = title_id;
		page.image_path = ""; // Placeholder
		
		// 添加标题内容项
		TutorialContentItem title_item;
		title_item.type = TutorialContentItem::TITLE;
		title_item.text_id = title_id;
		title_item.height = 0; // 自动计算
		page.content_items.push_back(title_item);
		
		// 添加正文内容项
		TutorialContentItem text_item;
		text_item.type = TutorialContentItem::TEXT;
		text_item.text_id = body_id;
		text_item.height = 0; // 自动计算
		page.content_items.push_back(text_item);
		
		// 为部分页面添加测试图片（使用现有 Sprite）
		if (i == 0 || i == 2) { // 第 1 页和第 3 页添加图片
			TutorialContentItem image_item;
			image_item.type = TutorialContentItem::IMAGE;
			image_item.sprite_id = SPR_SQUARE; // 使用现有的方块 sprite 作为占位
			image_item.text_id = INVALID_STRING_ID;
			image_item.height = 0; // 自动从 sprite 获取
			page.content_items.push_back(image_item);
		}
		
		tutorial_pages.push_back(page);
	}
}


void UpdateButtonState()
{
	if (current_page_index == 0) {
	disabled_state.previous_disabled=true;
	} 
	else {
		disabled_state.previous_disabled=false;
	}
	if(current_page_index >= tutorial_pages.size() - 1) {
	disabled_state.next_disabled=true;
	} 
	else {
		disabled_state.next_disabled=false;
	} 
}

bool OnTooltip(Point pt, WidgetID widget, TooltipCloseCondition close_cond) override
{
    if (widget == WID_TUT_PREVIOUS && disabled_state.previous_disabled) {
        GuiShowTooltips(this, GetEncodedString(STR_TUTORIAL_ALREADY_FIRST_PAGE), close_cond);
        return true;
    }
    
    if (widget == WID_TUT_NEXT && disabled_state.next_disabled) {
        GuiShowTooltips(this, GetEncodedString(STR_TUTORIAL_ALREADY_LAST_PAGE), close_cond);
        return true;
    }
    
    return false;
}

void UpdateUIForPage(uint index)
{
	if(index >= tutorial_pages.size()) return;
	UpdateButtonState();
	
	if(this->vscroll==nullptr)return; // 滚动条未初始化
	this->vscroll->SetPosition(0);//重置滚动条到顶部
	UpdateScrollbar();
	
	// 更新按钮禁用状态
	this->SetWidgetDisabledState(WID_TUT_PREVIOUS, disabled_state.previous_disabled);
    this->SetWidgetDisabledState(WID_TUT_NEXT, disabled_state.next_disabled);
	
	// 标记需要重绘的部件
	this->SetWidgetDirty(WID_TUT_PAGE_INDICATOR);
	this->SetWidgetDirty(WID_TUT_CONTENT);
    this->SetWidgetDirty(WID_TUT_PREVIOUS); 
	this->SetWidgetDirty(WID_TUT_NEXT);
};

void OnResize() override
{
	if(this->vscroll==nullptr)return; // 滚动条未初始化
	UpdateScrollbar();// 重新计算内容高度并更新滚动条
	// 防止滚动位置超出新的范围
    int max_pos = std::max(0, this->vscroll->GetCount() - this->vscroll->GetCapacity());
    if (this->vscroll->GetPosition() > max_pos) {
        this->vscroll->SetPosition(max_pos);
    }

	// 标记整个窗口需要重绘
	this->SetDirty();
}


};
/** Tutorial window widgets definition */
static constexpr std::initializer_list<NWidgetPart> _nested_tutorial_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_TUT_CAPTION), SetStringTip(STR_TUTORIAL_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_TUT_PANEL), SetResize(1, 1), SetMinimalSize(400, 300),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_TUT_CONTENT), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WID_TUT_SCROLLBAR), SetPadding(5),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_TUT_PAGE_INDICATOR), SetMinimalSize(400, 12), SetResize(1, 0), SetFill(1, 0), SetPadding(2, 5, 2, 5),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 0), SetPIP(5, 0, 5),
				NWidget(NWID_HORIZONTAL), SetPIP(5, 2, 5),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_PREVIOUS), SetMinimalSize(80, 20), SetStringTip(STR_TUTORIAL_PREV, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_NEXT), SetMinimalSize(80, 20), SetStringTip(STR_TUTORIAL_NEXT, STR_NULL),
					NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_CLOSE), SetMinimalSize(80, 20), SetStringTip(STR_TUTORIAL_FINISH, STR_TOOLTIP_CLOSE_WINDOW),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 0), SetPIP(5, 0, 5),
				NWidget(NWID_HORIZONTAL), SetPIP(5, 2, 5),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_DONT_SHOW), SetMinimalSize(150, 20), SetStringTip(STR_TUTORIAL_DONT_SHOW_AGAIN, STR_NULL),
					NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_FINISH), SetMinimalSize(80, 20), SetStringTip(STR_TUTORIAL_CLOSING_NOTE, STR_NULL),
					NWidget(WWT_RESIZEBOX, COLOUR_GREY),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_TUT_SCROLLBAR),
	EndContainer(),
};

/** Tutorial window description */
static WindowDesc _tutorial_window_desc(
	WDP_CENTER, {}, 500, 400,  // 默认宽度500，高度400
	WC_TUTORIAL, WC_NONE,
	{},
	_nested_tutorial_widgets
);


/**
 * Show the tutorial window.
 * @param force_show If true, ignore the tutorial_completed setting and show anyway.
 */
void ShowTutorialWindow(bool force_show)
{
	if (!force_show && _settings_client.gui.tutorial_completed) {
		return;
	}
	CloseWindowByClass(WC_TUTORIAL);
	new TutorialWindow(_tutorial_window_desc);
}

