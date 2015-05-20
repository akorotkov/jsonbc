#ifndef DICT_H_
#define DICT_H_

typedef struct
{
	char   *s;
	int		len;
} KeyName;

extern int32 getIdByName(KeyName name);
extern KeyName getNameById(int32 id);

#endif /* DICT_H_ */
