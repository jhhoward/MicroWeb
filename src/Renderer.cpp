#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "Renderer.h"
#include "Widget.h"
#include "Platform.h"
#include "App.h"

#define MIN_SCROLL_WIDGET_SIZE 8

Renderer::Renderer(App& inApp) 
	: app(inApp)
{
	pageTopWidgetIndex = 0;
	oldStatus = NULL;
	scrollPosition = 0;
}

void Renderer::Reset()
{
	pageTopWidgetIndex = 0;
	oldStatus = NULL;
	scrollPosition = 0;
	Platform::mouse->Hide();
	Platform::video->ClearWindow();
	Platform::mouse->Show();
}

int Renderer::GetMaxScrollPosition()
{
	int maxScroll = app.page.GetPageHeight() - Platform::video->windowHeight;
	if (maxScroll < 0)
	{
		maxScroll = 0;
	}
	return maxScroll;
}

void Renderer::Scroll(int delta)
{
	delta &= ~1;

	if (scrollPosition + delta < 0)
	{
		delta = -scrollPosition;
	}
	int maxScroll = GetMaxScrollPosition();
	if (scrollPosition + delta > maxScroll)
	{
		delta = maxScroll - scrollPosition;
	}

	if (delta == 0)
	{
		return;
	}

	Platform::mouse->Hide();

	if (delta < Platform::video->windowHeight && delta > -Platform::video->windowHeight)
	{
		Platform::video->ScrollWindow(delta);

		if (delta > 0)
		{
			Platform::video->SetScissorRegion(Platform::video->windowY + Platform::video->windowHeight - delta, Platform::video->windowY + Platform::video->windowHeight);
		}
		else
		{
			Platform::video->SetScissorRegion(Platform::video->windowY, Platform::video->windowY - delta);
		}
	}
	else
	{
		Platform::video->SetScissorRegion(Platform::video->windowY, Platform::video->windowY + Platform::video->windowHeight);
		Platform::video->ClearWindow();
	}

	scrollPosition += delta;

	RedrawScrollBar();

	if (delta < 0)
	{
		for (int n = pageTopWidgetIndex - 1; n >= 0; n--)
		{
			Widget* widget = &app.page.widgets[n];
			if (widget->y + widget->height < scrollPosition)
			{
				break;
			}
			pageTopWidgetIndex = n;
		}
	}
	else
	{
		for (int n = pageTopWidgetIndex; n < app.page.numWidgets; n++)
		{
			Widget* widget = &app.page.widgets[n];
			if (widget->y + widget->height < scrollPosition && n == pageTopWidgetIndex)
			{
				pageTopWidgetIndex++;
			}
			else break;
		}
	}

	for (int n = pageTopWidgetIndex; n < app.page.numWidgets; n++)
	{
		Widget* widget = &app.page.widgets[n];
		if (widget->y > scrollPosition + Platform::video->windowHeight)
		{
			break;
		}
		RenderWidget(widget);
	}

	Platform::mouse->Show();
}

void Renderer::RedrawScrollBar()
{
	int pageHeight = app.page.GetPageHeight();
	int windowHeight = Platform::video->windowHeight;
	int scrollWidgetSize = pageHeight < windowHeight ? windowHeight : windowHeight * windowHeight / pageHeight;
	if (scrollWidgetSize < MIN_SCROLL_WIDGET_SIZE)
		scrollWidgetSize = MIN_SCROLL_WIDGET_SIZE;
	int maxWidgetPosition = windowHeight - scrollWidgetSize;

	int maxScroll = GetMaxScrollPosition();
	int widgetPosition = maxScroll == 0 ? 0 : (int32_t)maxWidgetPosition * scrollPosition / maxScroll;

	Platform::video->DrawScrollBar(widgetPosition, scrollWidgetSize);
}

void Renderer::RenderWidget(Widget* widget)
{
	int baseY = Platform::video->windowY - scrollPosition;
	if (widget->text)
	{
		Platform::video->DrawString(widget->text, widget->x, widget->y + baseY, widget->style.fontSize, widget->style.fontStyle);
	}
}

void Renderer::OnPageWidgetsLoaded(Widget* widget, int count)
{
	Platform::mouse->Hide();
	Platform::video->SetScissorRegion(Platform::video->windowY, Platform::video->windowY + Platform::video->windowHeight);

	while(count)
	{
		RenderWidget(widget);
		count--;
		widget++;
	}

	if (app.page.pageHeight > Platform::video->windowHeight)
	{
		RedrawScrollBar();
	}
	Platform::mouse->Show();
}

Widget* Renderer::PickPageWidget(int x, int y)
{
	if (y < Platform::video->windowY || y > Platform::video->windowY + Platform::video->windowHeight || x > Platform::video->windowWidth)
	{
		return NULL;
	}
	return app.page.GetWidget(x, y - Platform::video->windowY + scrollPosition);
}

bool Renderer::IsOverPageWidget(Widget* widget, int x, int y)
{
	int adjustY = scrollPosition - Platform::video->windowY;

	return (x >= widget->x && y >= widget->y + adjustY && x < widget->x + widget->width && y < widget->y + widget->height + adjustY);
}

void Renderer::DrawStatus(const char* status)
{
	if (status != oldStatus)
	{
		Platform::mouse->Hide();

		Platform::video->DrawStatus(status ? status : "");

		Platform::mouse->Show();
		oldStatus = status;
	}
}

void Renderer::DrawAddress(const char* address)
{
	Platform::mouse->Hide();

	Platform::video->SetScissorRegion(0, Platform::video->windowY);
	Widget& addressBar = Platform::video->addressBar;

	Platform::video->ClearRect(addressBar.x + 1, addressBar.y + 1, addressBar.width - 2, addressBar.height - 2);
	Platform::video->DrawString(address, addressBar.x + 2, addressBar.y + 2);

	Platform::mouse->Show();

}
