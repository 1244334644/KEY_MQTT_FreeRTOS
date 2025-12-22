#include "delay.h"
#include "stm32f4xx.h"

#define TICKS_PER_MS (SystemCoreClock/1000)//TICKS_PER_MS = 168,000,000 / 1000 = 168,000 时钟周期/毫秒
#define TICKS_PER_US (SystemCoreClock/1000/1000)//TICKS_PER_US = 168,000,000 / 1,000,000 = 168 时钟周期/微秒

static volatile uint64_t cpu_tick_count;
static cpu_periodic_callback_t periodic_callback;


void cpu_tick_init(void)
{
	SysTick->LOAD  = TICKS_PER_MS;
	SysTick->VAL   = 0x00;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk;   
}


/**
 * @brief 获取高精度当前系统时间戳（以时钟周期为单位）
 * 
 * 该函数通过结合系统滴答计数器和当前SysTick计数值，
 * 实现了亚毫秒级精度的时间戳获取，为系统提供精确的时间基准。
 * 
 * 设计特点：
 * - 高精度：结合毫秒级计数器和亚毫秒级SysTick计数值
 * - 中断安全：使用无锁读取技术，避免中断禁用
 * - 实时性：适用于实时系统，不影响中断响应
 * 
 * @return uint64_t 当前时间戳（以时钟周期为单位）
 *                  从系统启动开始累计的时钟周期数
 */
uint64_t cpu_now(void)
{
	uint64_t now, last_count;  // now: 计算结果时间戳; last_count: 临时保存的计数器值
	
	// 使用do-while循环实现中断安全的无锁读取
	// 这种技术确保在读取过程中如果发生SysTick中断，会重新读取以保证数据一致性
	do{
		// 保存当前的系统滴答计数值
		// cpu_tick_count在SysTick_Handler中每毫秒递增TICKS_PER_MS
		last_count = cpu_tick_count;
		
		// 计算高精度时间戳：
		// cpu_tick_count: 已完成的完整毫秒数对应的时钟周期数
		// (SysTick->LOAD - SysTick->VAL): 当前毫秒内已过去的时钟周期数
		// SysTick->VAL从LOAD递减到0，所以LOAD-VAL得到已过去的周期数
		now = cpu_tick_count + (SysTick->LOAD - SysTick->VAL);
		
		// 检查在读取过程中是否发生了SysTick中断
		// 如果发生了中断，cpu_tick_count会被更新，需要重新读取保证数据一致性
	} while(last_count != cpu_tick_count);
	
	// 返回计算得到的高精度时间戳
	return now;
}

/**
 * @brief 获取系统启动以来的微秒级时间戳
 * 
 * 该函数将高精度时钟周期数转换为微秒单位的时间戳，
 * 为应用程序提供方便的时间测量接口。
 * 
 * 转换原理：
 * - 使用cpu_now()获取高精度时钟周期数
 * - 除以TICKS_PER_US将时钟周期转换为微秒
 * - TICKS_PER_US = SystemCoreClock / 1,000,000
 * 
 * 示例（假设SystemCoreClock = 168MHz）：
 * - TICKS_PER_US = 168,000,000 / 1,000,000 = 168
 * - 每微秒对应168个时钟周期
 * 
 * @return uint64_t 系统启动以来的微秒数
 *                  可用于时间间隔测量、超时检测等场景
 * 
 * @note 该函数依赖于cpu_now()的高精度时间获取
 * @see cpu_now() 获取高精度时钟周期数
 * @see cpu_get_ms() 获取毫秒级时间戳
 */
uint64_t cpu_get_us(void)
{
	// 将高精度时钟周期数转换为微秒单位
	// cpu_now(): 返回从系统启动开始的时钟周期总数
	// TICKS_PER_US: 每微秒对应的时钟周期数
	return cpu_now()/TICKS_PER_US;
}

uint64_t cpu_get_ms(void)
{
	return cpu_now()/TICKS_PER_MS;
}

/**
 * @brief 实现精确的微秒级延时
 * 
 * 该函数通过高精度时间测量实现精确的微秒级延时，
 * 适用于需要精确时间控制的嵌入式应用场景。
 * 
 * 延时原理：
 * 1. 记录当前时间戳作为起始点
 * 2. 循环检查当前时间与起始时间的差值
 * 3. 当差值达到指定的微秒数时退出循环
 * 
 * 算法特点：
 * - 高精度：基于cpu_now()的亚毫秒级时间测量
 * - 忙等待：使用循环等待，确保延时精度
 * - 中断安全：延时过程中允许中断响应
 * 
 * @param us 需要延时的微秒数
 *           - 范围：0 到 4,294,967,295 微秒（约71分钟）
 *           - 实际精度受系统时钟频率影响
 * 
 * @note 该函数是忙等待延时，会占用CPU资源
 * @warning 长时间延时会影响系统响应性
 * @see cpu_delay_ms() 毫秒级延时函数
 * @see cpu_now() 高精度时间获取函数
 */
void cpu_delay_us(uint32_t us)
{
	// 记录延时开始的时间戳（以时钟周期为单位）
	uint64_t now = cpu_now();
	
	// 忙等待循环，直到经过指定的微秒数
	// cpu_now() - now: 计算从开始到现在的时钟周期数

	// 假设需要延时10微秒（1680个时钟周期）
	// 开始时间 now = 1000000
	// 循环开始：
	// 第1次：当前时间=1000010，差值=10 < 1680，继续
	// 第2次：当前时间=1000020，差值=20 < 1680，继续
	// ...
	// 第168次：当前时间=1001680，差值=1680 >= 1680，退出循环

	// (uint64_t)us * TICKS_PER_US: 将微秒转换为对应的时钟周期数
	// 循环条件：已过去的时钟周期数 < 需要的时钟周期数
	while(cpu_now() - now < (uint64_t)us * TICKS_PER_US);
}

void cpu_delay_ms(uint32_t ms)
{
	uint64_t now = cpu_now();
	while(cpu_now() - now < (uint64_t)ms*TICKS_PER_MS);
}

void cpu_register_periodic_callback(cpu_periodic_callback_t callback)
{
	periodic_callback = callback;
}

void SysTick_Handler(void)
{
	cpu_tick_count+=TICKS_PER_MS;

	if (periodic_callback)
        periodic_callback();//本质执行的是cpu_periodic_callback()函数


}