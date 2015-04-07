/*
 * Aml nftl dev
 *
 * (C) 2012 8
 */

#include "../include/phynand.h"


struct aml_nand_device *aml_nand_dev = NULL;
int boot_device_flag = -1;


static struct class_attribute phydev_class_attrs[] = {
	__ATTR(info, S_IRUGO | S_IWUSR, show_nand_info, NULL),
	__ATTR(verify, S_IRUGO | S_IWUSR, NULL, verify_nand_page),
	__ATTR(dump, S_IRUGO | S_IWUSR, NULL, dump_nand_page),
	__ATTR(bbt_table, S_IRUGO | S_IWUSR, NULL, show_bbt_table),
	__ATTR(ioctl,       S_IRUGO | S_IWUSR, NULL,    nand_ioctl),
	__ATTR(page_read, S_IRUGO | S_IWUSR, NULL, nand_page_read),
	__ATTR(page_write, S_IRUGO | S_IWUSR, NULL, nand_page_write),
	__ATTR(version, S_IRUGO | S_IWUSR, show_amlnf_version_info, NULL),
	__ATTR_NULL
};

#if 0
static struct class_attribute logicdev_class_attrs[] = {
	__ATTR(part, S_IRUGO , show_part_struct, NULL),
	__ATTR(list, S_IRUGO , show_list, NULL),
	__ATTR(gcall, S_IRUGO , do_gc_all, NULL),
	__ATTR(gcone, S_IRUGO , do_gc_one, NULL),
	__ATTR(test, S_IRUGO | S_IWUSR , NULL, do_test),
	__ATTR_NULL
};

static struct class_attribute nfdev_class_attrs[] = {
	__ATTR(debug, S_IRUGO , nfdev_debug, NULL),
	__ATTR_NULL
};
#endif


#if 0
static struct class phydev_class = {
	.name = "amlphydev",
	.owner = THIS_MODULE,
	.suspend = phydev_cls_suspend,
	.resume = phydev_cls_resume,
};
#endif

int amlnf_pdev_register(struct amlnand_phydev *phydev)
{
	int ret = 0;
	/* phydev->dev.class = &phydev_class; */
	dev_set_name(&phydev->dev, phydev->name, 0);
	dev_set_drvdata(&phydev->dev, phydev);
	ret = device_register(&phydev->dev);
	if (ret != 0) {
		aml_nand_msg("device register failed for %s", phydev->name);
		aml_nand_free(phydev);
		goto exit_error0;
	}

	phydev->cls.name =
		aml_nand_malloc(strlen((const char *)phydev->name)+8);
	snprintf((char *)phydev->cls.name,
		(MAX_DEVICE_NAME_LEN+8),
		 "%s%s",
		 "phy_",
		 (char *)(phydev->name));
	phydev->cls.class_attrs = phydev_class_attrs;
	ret = class_register(&phydev->cls);
	if (ret) {
		aml_nand_msg("class register nand_class fail for %s",
			phydev->name);
		goto exit_error1;
	}
	return 0;
exit_error1:
	aml_nand_free(phydev->cls.name);
exit_error0:
	return ret;
}


static int amlnf_blk_open(struct block_device *bdev, fmode_t mode)
{
	return 0;
}

static void amlnf_blk_release(struct gendisk *disk, fmode_t mode)
{
	return;
}


static int amlnf_blk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	return 0;
}

static int amlnf_blk_ioctl(struct block_device *bdev, fmode_t mode,
			      unsigned int cmd, unsigned long arg)
{
	return 0;
}


static const struct block_device_operations amlnf_blk_ops = {
	.owner		= THIS_MODULE,
	.open		= amlnf_blk_open,
	.release	= amlnf_blk_release,
	.ioctl		= amlnf_blk_ioctl,
	.getgeo		= amlnf_blk_getgeo,
};

#define blk_queue_plugged(q)    test_bit(18, &(q)->queue_flags)


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int amlnf_logic_init(unsigned flag)
{
	struct amlnand_phydev *phydev = NULL;
	int ret = 0;

	aml_nand_msg("amlnand_add_nftl:");
	/* amlnand_show_dev_partition(aml_chip); */
	list_for_each_entry(phydev, &nphy_dev_list, list) {
		if (phydev == NULL)
			break;
		if (strncmp((char *)phydev->name,
			NAND_BOOT_NAME,
			strlen((const char *)NAND_BOOT_NAME))) {
			ret = add_ntd_partitions(phydev);
			if (ret < 0) {
				aml_nand_msg("nand add nftl failed");
				goto exit_error;
			}
		}
		if (!strncmp((char *)phydev->name,
			NAND_BOOT_NAME,
			strlen((const char *)NAND_BOOT_NAME))) {
			ret = boot_device_register(phydev);
			if (ret < 0) {
				aml_nand_msg("boot device registe failed");
				goto exit_error;
			}
		}
	}
exit_error:
	return ret;
}

static ssize_t nand_version_get(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	sprintf(buf, "%d", DRV_PHY_VERSION);
	return 0;
}

static void show_partition_table(struct partitions *table)
{
	int i = 0;
	struct partitions *par_table = NULL;

	aml_nand_msg("show partition table:");

	for (i = 0; i < MAX_PART_NUM; i++) {
		par_table = &table[i];
		if (par_table->size == -1) {
			aml_nand_msg("part: %d, name: %10s, size: %-4s",
				i,
				par_table->name,
				"end");
			break;
		} else
			aml_nand_msg("part: %d, name : %10s, size : %-4llx",
				i,
				par_table->name,
				par_table->size);
	}
	return;
}

static ssize_t nand_part_table_get(struct class *class,
			struct class_attribute *attr, char *buf)
{
	struct amlnand_phydev *phydev = NULL;
	struct amlnand_chip *aml_chip = NULL;
	struct nand_config *config = NULL;
	struct dev_para *dev_paramt = NULL;
	struct partitions *part_table = NULL;
	int i = 0, j = 0, k = 0, m = 0, tmp_num = 0;

	list_for_each_entry(phydev, &nphy_dev_list, list) {
		if ((phydev != NULL)
			&& (!strncmp((char *)phydev->name,
				NAND_CODE_NAME,
				strlen((const char *)NAND_CODE_NAME))))
			break;
	}

	aml_chip = (struct amlnand_chip *)phydev->priv;
	config =  aml_chip->config_ptr;

	part_table = aml_nand_malloc(MAX_PART_NUM*sizeof(struct partitions));
	if (!part_table) {
		aml_nand_msg("nand_part_table_get : malloc failed");
		return 0;
	}
	memset(part_table, 0x0, MAX_PART_NUM*sizeof(struct partitions));

	if (boot_device_flag == 1) {
		i += 1;
		tmp_num = i;
	}

	for (; i < PHY_DEV_NUM+1; i++) {
		dev_paramt = &config->dev_para[i];
		if ((!strncmp((char *)dev_paramt->name,
				NAND_CODE_NAME,
				strlen((const char *)NAND_CODE_NAME)))) {
			for (j = 0; j < dev_paramt->nr_partitions; j++) {
				memcpy(&(part_table[j].name),
					dev_paramt->partitions[j].name,
					MAX_NAND_PART_NAME_LEN);
				part_table[j].size =
					dev_paramt->partitions[j].size;
				part_table[j].offset =
					dev_paramt->partitions[j].offset;
				part_table[j].mask_flags =
					dev_paramt->partitions[j].mask_flags;
				/*
				aml_nand_msg("CODE: partiton name %s, size %llx,
				offset %llx maskflag %d",
				part_table[j].name,
				part_table[j].size,
				part_table[j].offset,
				part_table[j].mask_flags);
				*/
			}
			break;
		}
	}

	i = tmp_num;
	for (; i < PHY_DEV_NUM+1; i++) {
		dev_paramt = &config->dev_para[i];
		/*
		aml_nand_msg("cache : dev_paramt name %s ",dev_paramt->name);
		*/
		if ((!strncmp((char *)dev_paramt->name,
			NAND_CACHE_NAME,
			strlen((const char *)NAND_CACHE_NAME)))) {
			k = j++;
			for (j = 0; j < dev_paramt->nr_partitions; k++, j++) {
				memcpy(&(part_table[k].name),
					dev_paramt->partitions[j].name,
					MAX_NAND_PART_NAME_LEN);
				part_table[k].size =
					dev_paramt->partitions[j].size;
				part_table[k].offset =
					dev_paramt->partitions[j].offset;
				part_table[k].mask_flags =
					dev_paramt->partitions[j].mask_flags;
				/*
				aml_nand_msg("CODE: partiton name %s,size %llx,
				offset %llx maskflag %d",
				part_table[k].name,
				part_table[k].size,
				part_table[k].offset,
				part_table[k].mask_flags);
				*/
			}
			break;
		}
	}

	i = tmp_num;
	for (; i < PHY_DEV_NUM+1; i++) {
		dev_paramt = &config->dev_para[i];
		/*
		aml_nand_msg("dev_paramt name %s ",dev_paramt->name);
		*/
		if ((!strncmp((char *)dev_paramt->name,
			NAND_DATA_NAME,
			strlen((const char *)NAND_DATA_NAME)))) {
			m = k++;
			for (j = 0; j < dev_paramt->nr_partitions; j++) {
				memcpy(&(part_table[m].name),
					dev_paramt->partitions[j].name,
					MAX_NAND_PART_NAME_LEN);
				part_table[m].size =
					dev_paramt->partitions[j].size;
				part_table[m].offset =
					dev_paramt->partitions[j].offset;
				part_table[m].mask_flags =
					dev_paramt->partitions[j].mask_flags;
				/*
				aml_nand_msg("CODE:partiton name %s,size %llx,
				offset %llx maskflag %d",
				part_table[m].name,
				part_table[m].size,
				part_table[m].offset,
				part_table[m].mask_flags);
				*/
			}
			break;
		}
	}

	show_partition_table(part_table);
	memcpy(buf, part_table, MAX_PART_NUM*sizeof(struct partitions));

	kfree(part_table);
	part_table = NULL;

	return 0;
}

static ssize_t store_device_flag_get(struct class *class,
			struct class_attribute *attr, char *buf)
{
	sprintf(buf, "%d", boot_device_flag);
	return 0;
}

static struct class_attribute aml_version =
	__ATTR(version, S_IRUGO, nand_version_get, NULL);
static struct class_attribute aml_part_table =
	__ATTR(part_table, S_IRUGO, nand_part_table_get, NULL);
static struct class_attribute aml_store_device =
	__ATTR(store_device, S_IRUGO, store_device_flag_get, NULL);


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int amlnf_dev_init(unsigned int flag)
{
	struct amlnand_phydev *phydev = NULL;
	/* struct amlnf_dev* nf_dev = NULL; */
	struct class *aml_store_class = NULL;
	int ret = 0;

	list_for_each_entry(phydev, &nphy_dev_list, list) {
		if ((phydev != NULL)
			&& (strncmp((char *)phydev->name,
				NAND_BOOT_NAME,
				strlen((const char *)NAND_BOOT_NAME)))) {
			ret = amlnf_pdev_register(phydev);
			if (ret < 0) {
				aml_nand_msg("nand add nftl failed");
				goto exit_error0;
			}
		}
	}

	aml_store_class = class_create(THIS_MODULE, "aml_store");
	if (IS_ERR(aml_store_class)) {
		aml_nand_msg("amlnf_dev_init : class cread failed");
		ret =  -1;
		goto exit_error0;
	}

	ret = class_create_file(aml_store_class, &aml_version);
	if (ret) {
		aml_nand_msg("amlnf_dev_init :  cannot create sysfs file : ");
		goto out_class1;
	}
	ret = class_create_file(aml_store_class, &aml_part_table);
	if (ret) {
		aml_nand_msg("amlnf_dev_init : cannot create sysfs file : ");
		goto out_class2;
	}
	ret = class_create_file(aml_store_class, &aml_store_device);
	if (ret) {
		aml_nand_msg("amlnf_dev_init : cannot create sysfs file : ");
		goto out_class3;
	}

	return 0;
out_class3:
	class_remove_file(aml_store_class, &aml_part_table);
out_class2:
	class_remove_file(aml_store_class, &aml_version);
out_class1:
	class_destroy(aml_store_class);
exit_error0:
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_nand_dt_match[] = {
	{	.compatible = "amlogic,aml_nand",
	},
	{},
};

/*
static inline struct
aml_nand_device *aml_get_driver_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(amlogic_nand_dt_match, pdev->dev.of_node);
		return (struct aml_nand_device *)match->data;
	}
	return NULL;
}

static int get_nand_platform(struct aml_nand_device *aml_nand_dev,
		struct platform_device *pdev)
{
	int ret;
	int plat_num = 0;
	struct device_node *np = pdev->dev.of_node;

	if (pdev->dev.of_node) {
		of_node_get(np);
		ret = of_property_read_u32(np, "plat-num", &plat_num);
		if (ret) {
			aml_nand_msg("%s:%d,please config plat-num item\n",
				__func__,
				__LINE__);
			return -1;
		}
	}
	aml_nand_dbg("plat_num %d ", plat_num);

	return 0;
}
*/
#endif	/* CONFIG_OF */

/***
*boot_device_flag = 0 ; indicate spi+nand boot
*boot_device_flag = 1;  indicate nand  boot
***/
#if 1
int port_cfg_prase(void)
{
	int boot_flag;
	u32 cpu_type, port_value;

	port_value =
		amlnf_read_reg32(aml_nand_dev->platform_data->prot_cfg_reg);

	cpu_type = get_cpu_type();
	if (cpu_type >= MESON_CPU_MAJOR_ID_M8)
		port_value = ((((port_value >> 0x09) & 0x01) << 2) |
			((port_value >> 6) & 0x03));
	else
		port_value = (port_value & 0x07);

	if (cpu_type > MESON_CPU_MAJOR_ID_M8) {
		if (port_value == 0x05)
			boot_flag = SPI_BOOT_FLAG;
		if ((port_value == 0x03) || (port_value == 0x01))
			boot_flag = EMMC_BOOT_FLAG;
	} else {
		if ((port_value == 0x05) || (port_value == 0x04))
			boot_flag = SPI_BOOT_FLAG;
		if (port_value == 0x03)
			boot_flag = EMMC_BOOT_FLAG;
	}

	if ((port_value == 0x07) || (port_value == 0x06))
		boot_flag = NAND_BOOT_FLAG;
	if (port_value == 0x00)
		boot_flag = EMMC_BOOT_FLAG;

	return boot_flag;
}


int check_storage_device(void)
{
	int value = -1, port_cfg = -1;

	port_cfg = port_cfg_prase();
	value = boot_device_flag;

	if ((value == 0)
		|| (value == SPI_NAND_FLAG)
		|| (value == NAND_BOOT_FLAG)) {
		if ((value == 0) || (value == -1)) {
			if (port_cfg == NAND_BOOT_FLAG)
				boot_device_flag = 1;
			else if (port_cfg == EMMC_BOOT_FLAG)
				boot_device_flag = -1;
			else if (port_cfg == SPI_BOOT_FLAG)
				boot_device_flag = 0;
			else if (port_cfg == CARD_BOOT_FLAG)
				boot_device_flag = 1;
		} else if (value == SPI_NAND_FLAG)
			boot_device_flag = 0;
		else
			boot_device_flag = 1;
	} else
		boot_device_flag = -1;

	aml_nand_msg("boot_device_flag : %d", boot_device_flag);
	if ((boot_device_flag == 0) || (boot_device_flag == 1))
		return 0;
	else {
		boot_device_flag = value;
		return -NAND_FAILED;
	}
}
EXPORT_SYMBOL(check_storage_device);
#endif

static int  __init get_storage_device(char *str)
{
	int value = -1;

	if (kstrtoul(str, 16, (unsigned long *)&value))
		return -EINVAL;
	/*value = strtoul(str, NULL, 16);*/
	aml_nand_msg("get storage device: storage %s", str);
	aml_nand_msg("value=%d", value);
	boot_device_flag = value;
	return 0;
}
early_param("storage", get_storage_device);

static int amlnf_get_resource(struct platform_device *pdev)
{
	struct resource *res_mem, *res_irq;
	int size;

	/*
	getting Nand Platform Data that are set in dts files by of_xxx functions
	*/
#ifdef CONFIG_OF
/*
	pdev->dev.platform_data = aml_get_driver_data(pdev);
	aml_nand_msg("===========================================");
	aml_nand_msg("%s:%d,nand device tree ok,dev-name:%s\n",
		__func__,
		__LINE__,
		dev_name(&pdev->dev));
*/
#endif
/*
	ret = get_nand_platform(pdev->dev.platform_data, pdev);
*/
	/*aml_nand_dev = pdev->dev.platform_data;*/
	aml_nand_dev = kzalloc(sizeof(struct aml_nand_device), GFP_KERNEL);
	if (!aml_nand_dev) {
		aml_nand_msg("aml_nand_dev not exist\n");
		return -ENODEV;
	}

	aml_nand_dev->platform_data =
		kzalloc(sizeof(struct amlnf_platform_data), GFP_KERNEL);
	if (!aml_nand_dev->platform_data) {
		aml_nand_msg("malloc platform data fail\n");
		return -ENOMEM;
	}

	/*mapping PORT CONFIG register address*/
	aml_nand_dev->platform_data->prot_cfg_reg =
		devm_ioremap_nocache(&pdev->dev, PROT_CONFIG_REG, sizeof(int));
	if (aml_nand_dev->platform_data->prot_cfg_reg == NULL) {
		dev_err(&pdev->dev, "ioremap External Port Config IO fail\n");
		return -ENOMEM;
	}
	aml_nand_msg("prot_cfg_reg = 0x%0x\n",
		(unsigned int)aml_nand_dev->platform_data->prot_cfg_reg);

	/*getting nand platform device IORESOURCE_MEM*/
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		dev_err(&pdev->dev, "cannot obtain I/O memory region");
		return -ENODEV;
	}

	size = resource_size(res_mem);
	aml_nand_msg("%s : %0x  %0x\n", __func__, res_mem->start , size);

	/*mapping Nand Flash Controller register address*/
	/*request mem erea*/
	if (!devm_request_mem_region(&pdev->dev,
		res_mem->start,
		size,
		dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "Memory region busy\n");
		return -EBUSY;
	}

	/*mapping IO, nocache*/
	aml_nand_dev->platform_data->nf_reg_base =
		devm_ioremap_nocache(&pdev->dev,
		res_mem->start,
		size);
	if (aml_nand_dev->platform_data->nf_reg_base == NULL) {
		dev_err(&pdev->dev, "ioremap Nand Flash IO fail\n");
		return -ENOMEM;
	}

	/*mapping external Clock register address of Nand Flash Controller*/
	aml_nand_dev->platform_data->ext_clk_reg =
		devm_ioremap_nocache(&pdev->dev,
		NAND_CLK_CNTL,
		sizeof(int));
	if (aml_nand_dev->platform_data->ext_clk_reg == NULL) {
		dev_err(&pdev->dev, "ioremap External Nand Clock IO fail\n");
		return -ENOMEM;
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res_irq) {
		dev_err(&pdev->dev, "irq res get fail\n");
		return -ENODEV;
	}

	aml_nand_dev->platform_data->irq = res_irq->start;

	return 0;
}

static int amlnf_init(struct platform_device *pdev)
{
	int ret = 0;
	u32 flag = 0;

	/*initize nphy_dev_list*/
	INIT_LIST_HEAD(&nphy_dev_list);

	/*Nand physic init*/
	/*Amlogic Nand Flash Controller init and Nand chip init ......*/
	/*Scan or Build table,fbbt ,bbt, key and so on*/
	/*Creating physical partition, offset and size*/
	ret = amlnf_phy_init(flag, pdev);
	if (ret) {
		aml_nand_msg("nandphy_init failed and ret=0x%x", ret);
		goto exit_error0;
	}

	/*Nand logic init*/
	ret = amlnf_logic_init(flag);
	if (ret < 0) {
		aml_nand_msg("amlnf_add_nftl failed and ret=0x%x", ret);
		goto exit_error0;
	}

	ret = amlnf_dev_init(flag);
	if (ret < 0) {
		aml_nand_msg("amlnf_add_nftl failed and ret=0x%x", ret);
		goto exit_error0;
	}

exit_error0:
	return 0;/* ret;   //fix crash bug for error case. */
}

static int amlnf_driver_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = amlnf_get_resource(pdev);
	if (ret < 0) {
		aml_nand_msg("get resource fail!");
		return 0;
	}

	/*judge if it is nand boot device*/
	ret = check_storage_device();
	if (ret < 0) {
		aml_nand_msg("do not init nand");
		return 0;
	}

	/*Initializing Nand Flash*/
	amlnf_init(pdev);

	return 0;
}


static int amlnf_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static void amlnf_driver_shutdown(struct platform_device *pdev)
{
	struct amlnand_phydev *phy_dev = NULL;
	struct amlnand_chip *aml_chip = NULL;

	if (check_storage_device() < 0) {
		aml_nand_msg("without nand");
		return;
	}

	list_for_each_entry(phy_dev, &nphy_dev_list, list) {
		if (phy_dev) {
			aml_chip = (struct amlnand_chip *)phy_dev->priv;
			amlnand_get_device(aml_chip, CHIP_SHUTDOWN);
			phy_dev->option |= NAND_SHUT_DOWN;
			amlnand_release_device(aml_chip);
		}
	}
	return;
}

/* driver device registration */
static struct platform_driver amlnf_driver = {
	.probe = amlnf_driver_probe,
	.remove = amlnf_driver_remove,
	.shutdown = amlnf_driver_shutdown,
	.driver = {
		.name = DRV_AMLNFDEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = amlogic_nand_dt_match,
	},
};

static int __init amlnf_module_init(void)
{
	return platform_driver_register(&amlnf_driver);
}

static void __exit amlnf_module_exit(void)
{
	platform_driver_unregister(&amlnf_driver);
}

module_init(amlnf_module_init);
module_exit(amlnf_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AML NAND TEAM");
MODULE_DESCRIPTION("aml nand flash driver");


