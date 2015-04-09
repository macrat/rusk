var SCROLL_STEP = 12;

function onKeyPress(ev)
{
	if(ev.ctrlKey)
	{
		switch(ev.string)
		{
			case 'j':
				window.scrollBy(0, -SCROLL_STEP);
				break;
			case 'k':
				window.scrollBy(0,  SCROLL_STEP);
				break;
			case 'h':
				window.scrollBy(-SCROLL_STEP, 0);
				break;
			case 'l':
				window.scrollBy( SCROLL_STEP, 0);
		}
	}
}
document.addEventListener("keypress", onKeyPress, false);
